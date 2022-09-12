#ifndef WHISKER_SERVER_TASKS_H
#define WHISKER_SERVER_TASKS_H

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>
#include <glog/logging.h>
#include <json/json.h>
#include <whisker/message_log.h>
#include <whisker/task_queue.h>
#include <client.pb.h>
#include <console.pb.h>
#include "cartographer_map.h"

class ServerTasks final {
  public:
    ServerTasks(Json::Value config) : config(std::move(config)) {}

    template <typename RequestObservationFunc>
    void AddSensorClient(const std::string& sensor_id,
                         const whisker::proto::SensorClientInitMessage& init_message,
                         RequestObservationFunc&& request_observation_func) {
        std::unique_lock lock(data_mutex);
        const auto sensor =
                AddSensor(sensor_id, init_message, std::forward<RequestObservationFunc>(request_observation_func));
        if (sensor->vehicle->map) {
            LOG(INFO) << "Resuming observations from sensor '" << sensor_id
                      << "' because its vehicle is assigned to map '" << sensor->vehicle->map->map_id << "'";
            RequestObservation(sensor, true);
        }
    }

    template <typename InvocationFunc>
    void AddCapabilityClient(const std::string& client_id,
                             const whisker::proto::CapabilityClientInitMessage& init_message,
                             const InvocationFunc& invocation_func) {
        std::unique_lock lock(data_mutex);
        const auto vehicle = AddVehicle(init_message.vehicle_id());
        for (const auto& capability : init_message.capabilities()) {
            vehicle->capabilities[capability].try_emplace(client_id, invocation_func);
        }
    }

    void SubmitObservation(std::string&& sensor_id, const whisker::proto::ObservationMessage& observation) {
        std::shared_lock lock(data_mutex);
        const auto it = sensors.find(sensor_id);
        if (it != sensors.end()) {
            const auto& sensor = it->second;
            sensor->pending_observation = false;
            if (sensor->vehicle->map) {
                // generally better to copy the ObservationMessage than to move it from the message handler cache
                auto observation_ptr = std::make_shared<const whisker::proto::ObservationMessage>(observation);
                if (sensor->observation_log) {
                    sensor->observation_log->Write(observation_ptr);
                }
                sensor->vehicle->map->map_interface.SubmitObservation(std::move(sensor_id), sensor->data,
                                                                      std::move(observation_ptr));
                RequestObservation(sensor, false);
            }
        }
    }

    whisker::proto::ServerStateMessage GetServerState() {
        whisker::proto::ServerStateMessage msg;
        std::shared_lock lock(data_mutex);
        for (const auto& [map_id, map] : maps) {
            msg.add_map_ids(map_id);
        }
        for (const auto& [vehicle_id, vehicle] : vehicles) {
            auto& vehicle_data = (*msg.mutable_vehicles())[vehicle_id];
            if (vehicle->map) {
                vehicle_data.set_assigned_map_id(vehicle->map->map_id);
            }
            vehicle_data.set_keep_out_radius(vehicle->keep_out_radius);
            for (const auto& [capability, client_ids] : vehicle->capabilities) {
                vehicle_data.add_capabilities(capability);
            }
        }
        return msg;
    }

    template <typename Callback>
    void GetResourceFiles(Callback&& callback) {
        low_priority_task_queue.AddTask([this, callback = std::forward<Callback>(callback)] {
            whisker::proto::ResourceFilesMessage msg;
            for (const auto& entry : std::filesystem::directory_iterator{GetResourcePath({})}) {
                if (entry.is_regular_file() && (entry.path().extension() == saved_map_extension)) {
                    msg.add_maps(entry.path().filename().string());
                }
            }
            callback(msg);
        });
    }

    template <typename Callback>
    void GetMapData(const std::string& map_id, unsigned int have_version, Callback&& callback) {
        std::shared_lock lock(data_mutex);
        const auto map = maps.find(map_id);
        if (map != maps.end()) {
            map->second->map_interface.GetMapData(have_version, std::forward<Callback>(callback));
        }
    }

    template <typename Callback>
    void GetSubmapTextures(const whisker::proto::RequestSubmapTexturesMessage& request, const Callback& callback) {
        std::shared_lock lock(data_mutex);
        const auto map = maps.find(request.map_id());
        if (map != maps.end()) {
            for (const auto& submap_id : request.submap_ids()) {
                map->second->map_interface.GetSubmapTexture(submap_id.trajectory_id(), submap_id.index(), callback);
            }
        }
    }

    template <typename Callback>
    void GetVehiclePoses(const std::string& map_id, Callback&& callback) {
        std::shared_lock lock(data_mutex);
        const auto map = maps.find(map_id);
        if (map != maps.end()) {
            map->second->map_interface.GetVehiclePoses(std::forward<Callback>(callback));
        }
    }

    void InvokeCapability(const whisker::proto::InvokeCapabilityMessage& request) {
        std::shared_lock lock(data_mutex);
        const auto vehicle = vehicles.find(request.vehicle_id());
        if (vehicle != vehicles.end()) {
            const auto& capabilities = vehicle->second->capabilities;
            const auto capability_clients = capabilities.find(request.capability());
            if (capability_clients != capabilities.end()) {
                for (const auto& [capability_client_id, invocation_func] : capability_clients->second) {
                    invocation_func(request);
                }
            }
        }
    }

    void CreateMap(const std::string& map_id, bool use_overlapping_trimmer) {
        std::unique_lock lock(data_mutex);
        AddMap(map_id, use_overlapping_trimmer);
    }

    void DeleteMap(const std::string& map_id) {
        std::unique_lock lock(data_mutex);
        const auto map = maps.find(map_id);
        if (map != maps.end()) {
            for (const auto& [vehicle_id, vehicle] : vehicles) {
                if (vehicle->map && (vehicle->map->map_id == map_id)) {
                    vehicle->map.reset();
                }
            }
            QueueForDeletion(std::move(map->second));
            maps.erase(map);
        }
    }

    void SaveMap(const std::string& map_id) {
        std::string map_file_name = map_id;
        map_file_name.append("-");
        map_file_name.append(std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                    std::chrono::system_clock::now().time_since_epoch())
                                                    .count()));
        map_file_name.append(saved_map_extension);

        std::unique_lock lock(data_mutex);
        const auto map = maps.find(map_id);
        if (map != maps.end()) {
            auto map_file_path = GetResourcePath(map_file_name);
            if (!map_file_path.empty()) {
                for (const auto& [vehicle_id, vehicle] : vehicles) {
                    if (vehicle->map && (vehicle->map->map_id == map_id)) {
                        vehicle->map.reset();
                    }
                }
                map->second->map_interface.SaveState(std::move(map_file_path));
            }
        }
    }

    void LoadMap(const std::string& map_id,
                 const std::string& map_file_name,
                 bool is_frozen,
                 bool use_overlapping_trimmer) {
        auto map_file_path = GetResourcePath(map_file_name);
        if (!map_file_path.empty() && std::filesystem::is_regular_file(map_file_path) &&
            (std::filesystem::path(map_file_path).extension() == saved_map_extension)) {
            std::unique_lock lock(data_mutex);
            if (AddMap(map_id, use_overlapping_trimmer)) {
                maps.at(map_id)->map_interface.LoadState(std::move(map_file_path), is_frozen);
            } else {
                LOG(WARNING) << "Cannot load map '" << map_id << "' because a map with the same ID exists";
            }
        } else {
            LOG(WARNING) << "Map file '" << map_file_path << "' not found";
        }
    }

    void DeleteVehicle(const std::string& vehicle_id) {
        std::unique_lock lock(data_mutex);
        const auto it = vehicles.find(vehicle_id);
        if (it != vehicles.end()) {
            const auto& vehicle = it->second;
            for (const auto& sensor_weak_ptr : vehicle->sensors) {
                const auto sensor = sensors.find(sensor_weak_ptr.lock()->sensor_id);
                if (sensor != sensors.end()) {
                    QueueForDeletion(std::move(sensor->second));
                    sensors.erase(sensor);
                }
            }
            if (vehicle->map) {
                vehicle->map->map_interface.RemoveVehicle(vehicle_id);
            }
            vehicles.erase(it);
        }
    }

    void AssignVehicleToMap(const whisker::proto::RequestAssignVehicleToMapMessage& request) {
        const auto& vehicle_id = request.vehicle_id();
        const auto& map_id = request.map_id();

        std::unique_lock lock(data_mutex);
        const auto it = vehicles.find(vehicle_id);
        if (it != vehicles.end()) {
            const auto& vehicle = it->second;

            // no-op if vehicle is already assigned to requested map
            if (vehicle->map && (vehicle->map->map_id == map_id)) {
                return;
            }

            // remove vehicle from currently assigned map, or no-op if not assigned to a map
            if (vehicle->map) {
                vehicle->map->map_interface.RemoveVehicle(vehicle_id);
            }

            // add vehicle to new map (or no-op if it has no sensors, map doesn't exist, or map_id is an empty string)
            const auto map = maps.find(map_id);
            if (!vehicle->sensors.empty() && (map != maps.end())) {
                std::vector<CartographerMap::SensorIdAndType> sensor_ids;
                for (const auto& sensor_weak_ptr : vehicle->sensors) {
                    const auto sensor = sensor_weak_ptr.lock();
                    sensor_ids.emplace_back(sensor->sensor_id, sensor->data->sensor_type_case());
                    RequestObservation(sensor, false);
                }
                map->second->map_interface.AddVehicle(vehicle_id, std::move(sensor_ids), request.initial_pose(),
                                                      request.allow_global_localization(),
                                                      request.use_localization_trimmer());
                vehicle->map = map->second;
            } else {
                LOG_IF(WARNING, vehicle->sensors.empty()) << "Vehicle '" << vehicle_id << "' has no sensors";
                vehicle->map.reset();
            }
        }
    }

    void StartObservationLog(const std::string& vehicle_id) {
        std::string log_file_suffix = "-";
        log_file_suffix.append(std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                      std::chrono::system_clock::now().time_since_epoch())
                                                      .count()));
        log_file_suffix.append(observation_log_extension);

        std::unique_lock lock(data_mutex);
        const auto vehicle = vehicles.find(vehicle_id);
        if (vehicle != vehicles.end()) {
            for (const auto& sensor_weak_ptr : vehicle->second->sensors) {
                const auto sensor = sensor_weak_ptr.lock();
                if (!sensor->observation_log) {
                    const auto log_file_path = GetResourcePath(sensor->sensor_id + log_file_suffix);
                    if (!log_file_path.empty()) {
                        sensor->observation_log = whisker::MessageLogWriter::CreateInstance(log_file_path);
                        sensor->observation_log->Write(sensor->data);
                    }
                }
            }
        }
    }

    void StopObservationLog(const std::string& vehicle_id) {
        std::unique_lock lock(data_mutex);
        const auto vehicle = vehicles.find(vehicle_id);
        if (vehicle != vehicles.end()) {
            for (const auto& sensor_weak_ptr : vehicle->second->sensors) {
                QueueForDeletion(std::move(sensor_weak_ptr.lock()->observation_log));
            }
        }
    }

  private:
    struct Map;
    struct Vehicle;
    struct Sensor;

    struct Map {
        Map(const std::string& map_id, const Json::Value& config, bool use_overlapping_trimmer)
                : map_id(map_id), map_interface(map_id, config, use_overlapping_trimmer) {}
        std::string map_id;
        CartographerMap map_interface;
    };

    struct Vehicle {
        std::string vehicle_id;
        float keep_out_radius = 0;
        std::unordered_map<
                std::string,
                std::unordered_map<std::string, std::function<void(const whisker::proto::InvokeCapabilityMessage&)>>>
                capabilities;  // map<capability name, map<client ids that provide the capability, invocation func>>

        std::shared_ptr<Map> map;                    // Map that this Vehicle is assigned to (or null)
        std::vector<std::weak_ptr<Sensor>> sensors;  // Sensors that belong to this Vehicle
    };

    struct Sensor {
        std::string sensor_id;
        std::shared_ptr<const whisker::proto::SensorClientInitMessage> data;
        std::function<void()> request_observation_func;
        std::shared_ptr<whisker::MessageLogWriter> observation_log;
        std::atomic_bool pending_observation = false;

        std::shared_ptr<Vehicle> vehicle;  // Vehicle that this Sensor is on
    };

    bool AddMap(const std::string& map_id, bool use_overlapping_trimmer) {
        if (!map_id.empty() && (maps.count(map_id) == 0)) {
            maps.try_emplace(map_id, std::make_shared<Map>(map_id, config["cartographer"], use_overlapping_trimmer));
            return true;
        }
        return false;
    }

    std::shared_ptr<Vehicle> AddVehicle(const std::string& vehicle_id) {
        const auto [it, emplaced] = vehicles.try_emplace(vehicle_id, std::make_shared<Vehicle>());
        if (emplaced) {
            it->second->vehicle_id = vehicle_id;
        }
        return it->second;
    }

    template <typename RequestObservationFunc>
    std::shared_ptr<Sensor> AddSensor(const std::string& sensor_id,
                                      const whisker::proto::SensorClientInitMessage& init_message,
                                      RequestObservationFunc&& request_observation_func) {
        const auto [it, emplaced] = sensors.try_emplace(sensor_id, std::make_shared<Sensor>());
        if (emplaced) {
            const auto& sensor = it->second;
            sensor->sensor_id = sensor_id;
            sensor->data = std::make_shared<const whisker::proto::SensorClientInitMessage>(init_message);
            sensor->request_observation_func = std::forward<RequestObservationFunc>(request_observation_func);
            sensor->vehicle = AddVehicle(init_message.vehicle_id());
            sensor->vehicle->keep_out_radius =
                    std::max(sensor->vehicle->keep_out_radius, init_message.keep_out_radius());
            sensor->vehicle->sensors.emplace_back(sensor);
        }
        return it->second;
    }

    // to minimize latency, delete objects with long-running destructors on another thread
    template <typename T>
    void QueueForDeletion(std::shared_ptr<T>&& object_ptr) {
        low_priority_task_queue.AddTask([ptr = std::move(object_ptr)] {});
    }

    std::string GetResourcePath(const std::string& resource_name) const {
        const auto resource_dir = std::filesystem::absolute(config["resource_dir"].asString());
        auto resource_path = resource_dir / resource_name;
        if (resource_path.parent_path() != resource_dir) {
            LOG(WARNING) << "The path " << resource_path << " is outside the resource directory";
            resource_path.clear();
        } else if (!std::filesystem::exists(resource_dir)) {
            std::filesystem::create_directories(resource_dir);
        }
        return resource_path.string();
    }

    static void RequestObservation(const std::shared_ptr<Sensor>& sensor, bool force) {
        const auto already_pending = sensor->pending_observation.exchange(true);
        if (!already_pending || force) {
            sensor->request_observation_func();
        }
    }

    static constexpr std::string_view saved_map_extension = ".pbstream";
    static constexpr std::string_view observation_log_extension = ".obslog";

    std::unordered_map<std::string, std::shared_ptr<Map>> maps;
    std::unordered_map<std::string, std::shared_ptr<Vehicle>> vehicles;
    std::unordered_map<std::string, std::shared_ptr<Sensor>> sensors;
    std::shared_mutex data_mutex;
    const Json::Value config;
    whisker::TaskQueue low_priority_task_queue;
};

#endif  // WHISKER_SERVER_TASKS_H
