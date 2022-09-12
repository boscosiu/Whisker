#ifndef WHISKER_CARTOGRAPHER_MAP_H
#define WHISKER_CARTOGRAPHER_MAP_H

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <cartographer/mapping/id.h>
#include <cartographer/mapping/map_builder_interface.h>
#include <cartographer/mapping/submaps.h>
#include <cartographer/mapping/proto/trajectory_builder_options.pb.h>
#include <cartographer/transform/rigid_transform.h>
#include <json/json.h>
#include <whisker/task_queue.h>
#include <client.pb.h>
#include <console.pb.h>

class CartographerMap final {
  public:
    using SensorIdAndType = std::pair<std::string, whisker::proto::SensorClientInitMessage::SensorTypeCase>;

    CartographerMap(std::string id, Json::Value cfg, bool use_overlapping_trimmer);
    ~CartographerMap();

    CartographerMap(const CartographerMap&) = delete;
    CartographerMap& operator=(const CartographerMap&) = delete;

    void AddVehicle(std::string vehicle_id,
                    std::vector<SensorIdAndType> sensor_ids,
                    whisker::proto::Transform initial_pose,
                    bool allow_global_localization,
                    bool use_localization_trimmer);
    void RemoveVehicle(std::string vehicle_id);

    void GetMapData(unsigned int have_version, std::function<void(const whisker::proto::MapDataMessage&)> callback);
    void GetSubmapTexture(int trajectory_id,
                          int index,
                          std::function<void(const whisker::proto::SubmapTextureMessage&)> callback);
    void GetVehiclePoses(std::function<void(const whisker::proto::VehiclePosesMessage&)> callback);

    void SubmitObservation(std::string sensor_id,
                           std::shared_ptr<const whisker::proto::SensorClientInitMessage> sensor_data,
                           std::shared_ptr<const whisker::proto::ObservationMessage> observation);

    void SaveState(std::string state_file_path);
    void LoadState(std::string state_file_path, bool is_frozen);

  private:
    struct VehicleData {
        int trajectory_id;
        cartographer::transform::Rigid3d local_pose;
        const void* latest_submap_ptr = nullptr;
    };

    void DoFinalOptimization();

    static void CreateSubmapTexture(const std::shared_ptr<const cartographer::mapping::Submap>& submap,
                                    whisker::proto::SubmapTextureMessage& texture_msg);

    const std::string map_id;
    const Json::Value config;
    cartographer::mapping::proto::TrajectoryBuilderOptions trajectory_builder_options;
    std::unique_ptr<cartographer::mapping::MapBuilderInterface> map_builder;
    std::atomic_uint map_data_version = 1;
    whisker::TaskQueue task_queue;
    std::unordered_map<std::string, VehicleData> vehicles;
    whisker::proto::MapDataMessage map_data_cache;
    std::unordered_map<cartographer::mapping::SubmapId, whisker::proto::SubmapTextureMessage> submap_texture_cache;
};

#endif  // WHISKER_CARTOGRAPHER_MAP_H
