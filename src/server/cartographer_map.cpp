#include "cartographer_map.h"
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <unordered_set>
#include <cartographer/common/configuration_file_resolver.h>
#include <cartographer/common/lua_parameter_dictionary.h>
#include <cartographer/common/time.h>
#include <cartographer/mapping/map_builder.h>
#include <cartographer/mapping/trajectory_builder_interface.h>
#include <cartographer/mapping/2d/submap_2d.h>
#include <cartographer/mapping/2d/xy_index.h>
#include <cartographer/sensor/imu_data.h>
#include <cartographer/sensor/rangefinder_point.h>
#include <cartographer/sensor/timed_point_cloud_data.h>
#include <cartographer/transform/transform.h>
#include <Eigen/Geometry>
#include <glog/logging.h>
#include <png.h>

CartographerMap::CartographerMap(std::string id, Json::Value cfg, bool use_overlapping_trimmer)
        : map_id(std::move(id)), config(std::move(cfg)) {
    const auto config_file = config["config_file"].asString();
    const auto base_config_dir = config["base_config_dir"].asString();

    LOG(INFO) << "Creating CartographerMap for map '" << map_id << "' with Cartographer config file "
              << std::filesystem::absolute(config_file).lexically_normal() << " and base config at "
              << std::filesystem::absolute(base_config_dir).lexically_normal();

    std::ifstream config_fstream(config_file);
    CHECK(config_fstream) << "Error opening Cartographer config file";

    cartographer::common::LuaParameterDictionary parameters(
            {std::istreambuf_iterator<char>{config_fstream}, std::istreambuf_iterator<char>{}},
            std::make_unique<cartographer::common::ConfigurationFileResolver>(
                    std::vector<std::string>{base_config_dir}));

    trajectory_builder_options =
            cartographer::mapping::CreateTrajectoryBuilderOptions(parameters.GetDictionary("trajectory_builder").get());

    auto map_builder_options =
            cartographer::mapping::CreateMapBuilderOptions(parameters.GetDictionary("map_builder").get());
    const auto pose_graph_options = map_builder_options.mutable_pose_graph_options();

    if (use_overlapping_trimmer && !pose_graph_options->has_overlapping_submaps_trimmer_2d()) {
        const auto trimmer_options = pose_graph_options->mutable_overlapping_submaps_trimmer_2d();
        trimmer_options->set_fresh_submaps_count(config["overlapping_trimmer_fresh_submaps_count"].asInt());
        trimmer_options->set_min_covered_area(config["overlapping_trimmer_min_covered_area"].asDouble());
        trimmer_options->set_min_added_submaps_count(config["overlapping_trimmer_min_added_submaps_count"].asInt());
    }

    map_builder = cartographer::mapping::CreateMapBuilder(map_builder_options);
    map_builder->pose_graph()->SetGlobalSlamOptimizationCallback(
            [this](const auto& last_optimized_submaps, const auto& last_optimized_nodes) { ++map_data_version; });

    map_data_cache.set_map_id(map_id);
}

CartographerMap::~CartographerMap() {
    task_queue.AddTask([this] { DoFinalOptimization(); });
    task_queue.FinishQueueSync();  // do this in destructor to ensure class members outlive queue
}

void CartographerMap::AddVehicle(std::string vehicle_id,
                                 std::vector<SensorIdAndType> sensor_ids,
                                 whisker::proto::Transform initial_pose,
                                 bool allow_global_localization,
                                 bool use_localization_trimmer) {
    task_queue.AddTask([this, vehicle_id = std::move(vehicle_id), sensor_ids = std::move(sensor_ids),
                        initial_pose = std::move(initial_pose), allow_global_localization, use_localization_trimmer] {
        const auto [it, emplaced] = vehicles.try_emplace(vehicle_id);
        if (emplaced) {
            bool using_imu = false;

            std::set<cartographer::mapping::MapBuilderInterface::SensorId> sensors;
            for (const auto& sensor_id : sensor_ids) {
                switch (sensor_id.second) {
                    case whisker::proto::SensorClientInitMessage::kImuProperties: {
                        using_imu = true;
                        sensors.insert({cartographer::mapping::MapBuilderInterface::SensorId::SensorType::IMU,
                                        sensor_id.first});
                    } break;

                    case whisker::proto::SensorClientInitMessage::kLidarProperties: {
                        sensors.insert({cartographer::mapping::MapBuilderInterface::SensorId::SensorType::RANGE,
                                        sensor_id.first});
                    } break;

                    case whisker::proto::SensorClientInitMessage::SENSOR_TYPE_NOT_SET: {
                        LOG(WARNING) << "Sensor '" << sensor_id.first << "' did not provide its sensor properties";
                    } break;
                }
            }

            auto options = trajectory_builder_options;
            options.mutable_trajectory_builder_2d_options()->set_use_imu_data(using_imu);
            options.mutable_trajectory_builder_2d_options()->set_use_online_correlative_scan_matching(!using_imu);
            *options.mutable_initial_global_pose() = cartographer::transform::ToProto(
                    cartographer::transform::Rigid3d{Eigen::Vector3d{initial_pose.x(), initial_pose.y(), 0},
                                                     Eigen::AngleAxisd{initial_pose.r(), Eigen::Vector3d::UnitZ()}});
            options.set_allow_global_localization(allow_global_localization);
            if (use_localization_trimmer) {
                options.mutable_pure_localization_trimmer()->set_max_submaps_to_keep(
                        config["pure_localization_num_submaps"].asInt());
            }

            it->second.trajectory_id = map_builder->AddTrajectoryBuilder(
                    sensors, options,
                    // callback that is invoked when TrajectoryBuilderInterface::AddSensorData() processes a scan
                    [this, vehicle_id](const auto& trajectory, const auto& time, const auto& local_pose,
                                       const auto& range_data, const auto& insertion_result) {
                        const auto vehicle = vehicles.find(vehicle_id);
                        vehicle->second.local_pose = local_pose;
                        if (insertion_result) {
                            const auto latest_submap_ptr = insertion_result->insertion_submaps.back().get();
                            // follow Cartographer's logic of comparing submap pointer values to detect changes
                            if (vehicle->second.latest_submap_ptr != latest_submap_ptr) {
                                vehicle->second.latest_submap_ptr = latest_submap_ptr;
                                ++map_data_version;
                            }
                        }
                    });

            LOG(INFO) << "Added vehicle '" << vehicle_id << "' as trajectory " << it->second.trajectory_id << " "
                      << (using_imu ? "with" : "without") << " IMU"
                      << (use_localization_trimmer ? ", using localization trimmer" : "");
        }
    });
}

void CartographerMap::RemoveVehicle(std::string vehicle_id) {
    task_queue.AddTask([this, vehicle_id = std::move(vehicle_id)] {
        const auto vehicle = vehicles.find(vehicle_id);
        if (vehicle != vehicles.end()) {
            map_builder->FinishTrajectory(vehicle->second.trajectory_id);
            vehicles.erase(vehicle);
        }
    });
}

void CartographerMap::GetMapData(unsigned int have_version,
                                 std::function<void(const whisker::proto::MapDataMessage&)> callback) {
    task_queue.AddTask([this, have_version, callback = std::move(callback)] {
        const auto is_new_map = (have_version != map_data_version);

        map_data_cache.set_is_new_map_version(is_new_map);

        if (is_new_map) {
            // set this before calling GetAllSubmapPoses() to prevent data race
            map_data_cache.set_map_version(map_data_version);
        } else {
            map_data_cache.clear_map_version();
        }

        map_data_cache.clear_submaps();
        const auto& submaps = map_builder->pose_graph()->GetAllSubmapPoses();
        for (const auto& submap : submaps) {
            const auto submap_msg = map_data_cache.add_submaps();
            submap_msg->mutable_submap_id()->set_trajectory_id(submap.id.trajectory_id);
            submap_msg->mutable_submap_id()->set_index(submap.id.submap_index);
            submap_msg->set_version(submap.data.version);
            if (is_new_map) {
                submap_msg->mutable_global_pose()->set_x(submap.data.pose.translation().x());
                submap_msg->mutable_global_pose()->set_y(submap.data.pose.translation().y());
                submap_msg->mutable_global_pose()->set_r(cartographer::transform::GetYaw(submap.data.pose));
            }
        }

        callback(map_data_cache);

        // delete cached textures of submaps that no longer exist (submap_texture_cache.size() serves as a heuristic)
        if (is_new_map && (submap_texture_cache.size() > submaps.size())) {
            std::unordered_set<cartographer::mapping::SubmapId> cached_ids(submap_texture_cache.size());
            for (const auto& [id, texture] : submap_texture_cache) {
                cached_ids.emplace(id);
            }
            for (const auto& submap : submaps) {
                cached_ids.erase(submap.id);
            }
            for (const auto& obsolete_id : cached_ids) {
                submap_texture_cache.erase(obsolete_id);
            }
        }
    });
}

void CartographerMap::GetSubmapTexture(int trajectory_id,
                                       int index,
                                       std::function<void(const whisker::proto::SubmapTextureMessage&)> callback) {
    task_queue.AddTask([this, trajectory_id, index, callback = std::move(callback)] {
        const cartographer::mapping::SubmapId submap_id(trajectory_id, index);
        const auto submap = map_builder->pose_graph()->GetSubmapData(submap_id).submap;
        if (submap) {
            const auto [it, emplaced] = submap_texture_cache.try_emplace(submap_id);
            auto& texture_msg = it->second;
            if (emplaced) {
                texture_msg.set_map_id(map_id);
                texture_msg.mutable_submap_id()->set_trajectory_id(trajectory_id);
                texture_msg.mutable_submap_id()->set_index(index);
                CreateSubmapTexture(submap, texture_msg);
            } else if (texture_msg.version() != submap->num_range_data()) {
                CreateSubmapTexture(submap, texture_msg);
            }
            callback(texture_msg);
        }
    });
}

void CartographerMap::GetVehiclePoses(std::function<void(const whisker::proto::VehiclePosesMessage&)> callback) {
    task_queue.AddTask([this, callback = std::move(callback)] {
        whisker::proto::VehiclePosesMessage msg;
        msg.set_map_id(map_id);
        for (const auto& [vehicle_id, vehicle_data] : vehicles) {
            const auto pose = map_builder->pose_graph()->GetLocalToGlobalTransform(vehicle_data.trajectory_id) *
                              vehicle_data.local_pose;
            const auto vehicle_pose = msg.add_vehicle_poses();
            vehicle_pose->set_vehicle_id(vehicle_id);
            vehicle_pose->mutable_pose()->set_x(pose.translation().x());
            vehicle_pose->mutable_pose()->set_y(pose.translation().y());
            vehicle_pose->mutable_pose()->set_r(cartographer::transform::GetYaw(pose));
        }
        callback(msg);
    });
}

void CartographerMap::SubmitObservation(std::string sensor_id,
                                        std::shared_ptr<const whisker::proto::SensorClientInitMessage> sensor_data,
                                        std::shared_ptr<const whisker::proto::ObservationMessage> observation) {
    task_queue.AddTask([this, sensor_id = std::move(sensor_id), sensor_data = std::move(sensor_data),
                        observation = std::move(observation)] {
        const auto vehicle = vehicles.find(sensor_data->vehicle_id());
        if (vehicle != vehicles.end()) {
            const cartographer::common::Time timestamp(
                    cartographer::common::FromMilliseconds(observation->timestamp()));

            switch (observation->sensor_type_case()) {
                case whisker::proto::ObservationMessage::kImuObservation: {
                    const auto& imu_properties = sensor_data->imu_properties();
                    const auto& imu_observation = observation->imu_observation();

                    const auto sensor_rotation = cartographer::transform::RollPitchYaw(
                            imu_properties.roll(), imu_properties.pitch(), imu_properties.yaw());

                    cartographer::sensor::ImuData imu_data;
                    imu_data.time = timestamp;
                    imu_data.linear_acceleration =
                            sensor_rotation * Eigen::Vector3d{imu_observation.linear_acceleration_x(),
                                                              imu_observation.linear_acceleration_y(),
                                                              imu_observation.linear_acceleration_z()};
                    imu_data.angular_velocity = sensor_rotation * Eigen::Vector3d{imu_observation.angular_velocity_x(),
                                                                                  imu_observation.angular_velocity_y(),
                                                                                  imu_observation.angular_velocity_z()};

                    map_builder->GetTrajectoryBuilder(vehicle->second.trajectory_id)
                            ->AddSensorData(sensor_id, imu_data);
                } break;

                case whisker::proto::ObservationMessage::kLidarObservation: {
                    const auto& lidar_properties = sensor_data->lidar_properties();
                    const auto& lidar_observation = observation->lidar_observation();

                    const cartographer::transform::Rigid3d sensor_pose(
                            {lidar_properties.position().x(), lidar_properties.position().y(), 0},
                            Eigen::AngleAxisd{lidar_properties.position().r(), Eigen::Vector3d::UnitZ()});

                    cartographer::sensor::TimedPointCloudData point_data;
                    point_data.time = timestamp;
                    point_data.origin = sensor_pose.translation().cast<float>();

                    const auto secs_between_measurements = lidar_properties.angular_resolution() / (2 * M_PI) /
                                                           lidar_properties.rotations_per_second();

                    Eigen::AngleAxisd rotation(lidar_properties.starting_angle(), Eigen::Vector3d::UnitZ());

                    point_data.ranges.reserve(lidar_observation.measurements_size());
                    for (auto i = 0; i < lidar_observation.measurements_size(); ++i) {
                        point_data.ranges.emplace_back(cartographer::sensor::TimedRangefinderPoint{
                                (sensor_pose *
                                 (rotation * (lidar_observation.measurements(i) / 1000.0 * Eigen::Vector3d::UnitX())))
                                        .cast<float>(),
                                static_cast<float>((i + 1 - lidar_observation.measurements_size()) *
                                                   secs_between_measurements)});
                        rotation.angle() += lidar_properties.angular_resolution();
                    }

                    map_builder->GetTrajectoryBuilder(vehicle->second.trajectory_id)
                            ->AddSensorData(sensor_id, point_data);
                } break;

                case whisker::proto::ObservationMessage::SENSOR_TYPE_NOT_SET: {
                    LOG(WARNING) << "Received an empty observation from sensor '" << sensor_id << "'";
                } break;
            }
        }
    });
}

void CartographerMap::SaveState(std::string state_file_path) {
    task_queue.AddTask([this, state_file_path = std::move(state_file_path)] {
        DoFinalOptimization();
        if (map_builder->SerializeStateToFile(true, state_file_path)) {
            LOG(INFO) << "Saved state of map '" << map_id << "' to "
                      << std::filesystem::absolute(state_file_path).lexically_normal();
        } else {
            LOG(WARNING) << "Error saving state of map '" << map_id << "'";
        }
    });
}

void CartographerMap::LoadState(std::string state_file_path, bool is_frozen) {
    task_queue.AddTask([this, state_file_path = std::move(state_file_path), is_frozen] {
        map_builder->LoadStateFromFile(state_file_path, is_frozen);
        map_builder->pose_graph()->RunFinalOptimization();
        LOG(INFO) << "Loaded map state from " << std::filesystem::absolute(state_file_path).lexically_normal()
                  << " into map '" << map_id << "'";
    });
}

void CartographerMap::DoFinalOptimization() {
    for (const auto& [vehicle_id, vehicle_data] : vehicles) {
        map_builder->FinishTrajectory(vehicle_data.trajectory_id);
    }
    vehicles.clear();
    map_builder->pose_graph()->RunFinalOptimization();
}

void CartographerMap::CreateSubmapTexture(const std::shared_ptr<const cartographer::mapping::Submap>& submap,
                                          whisker::proto::SubmapTextureMessage& texture_msg) {
    const auto grid = std::static_pointer_cast<const cartographer::mapping::Submap2D>(submap)->grid();

    // use a smaller cropped bounding box containing all known cells to produce the submap texture
    Eigen::Array2i cropped_offset;
    cartographer::mapping::CellLimits cropped_limits;
    grid->ComputeCroppedLimits(&cropped_offset, &cropped_limits);

    const auto png_width = cropped_limits.num_x_cells;
    const auto png_height = cropped_limits.num_y_cells;
    const auto resolution = grid->limits().resolution();

    texture_msg.set_version(submap->num_range_data());
    texture_msg.set_resolution(resolution);

    // this is the transform from the submap's local pose to the center of the generated PNG
    // (the -M_PI/2 rotation is to compensate for the grid's rotated layout)
    texture_msg.mutable_submap_pose()->set_x(grid->limits().max().x() - submap->local_pose().translation().x() -
                                             (resolution * (cropped_offset.y() + (png_height / 2.0))));
    texture_msg.mutable_submap_pose()->set_y(grid->limits().max().y() - submap->local_pose().translation().y() -
                                             (resolution * (cropped_offset.x() + (png_width / 2.0))));
    texture_msg.mutable_submap_pose()->set_r(-M_PI / 2);

    const auto png_out = texture_msg.mutable_texture();

    thread_local std::vector<std::uint8_t> submap_texture_buffer;
    submap_texture_buffer.resize(png_width * png_height);

    auto cursor = submap_texture_buffer.data();
    for (const auto& cropped_index : cartographer::mapping::XYIndexRangeIterator(cropped_limits)) {
        const auto cell = cropped_offset + cropped_index;
        if (grid->IsKnown(cell)) {
            *(cursor++) = cartographer::mapping::ProbabilityToLogOddsInteger(1 - grid->GetCorrespondenceCost(cell));
        } else {
            *(cursor++) = 0;
        }
    }

    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    image.width = png_width;
    image.height = png_height;
    image.format = PNG_FORMAT_GRAY;

    auto output_size = PNG_IMAGE_PNG_SIZE_MAX(image);
    png_out->resize(output_size);
    CHECK(png_image_write_to_memory(&image, png_out->data(), &output_size, 0, submap_texture_buffer.data(), png_width,
                                    nullptr));
    png_out->resize(output_size);
}
