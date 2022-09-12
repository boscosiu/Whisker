#ifndef WHISKER_HOKUYO_H
#define WHISKER_HOKUYO_H

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <json/json.h>
#include <whisker/overwriting_buffer.h>
#include <whisker/sensor_time_sync.h>
#include <client.pb.h>

class UrgSensor;

class Hokuyo final {
  public:
    Hokuyo(const Json::Value& config);
    ~Hokuyo();

    Hokuyo(const Hokuyo&) = delete;
    Hokuyo& operator=(const Hokuyo&) = delete;

    whisker::proto::LidarSensorProperties GetSensorProperties() const;
    whisker::proto::ObservationMessage GetLatestObservation();

  private:
    struct Observation {
        long timestamp;  // milliseconds
        int num_distances;
        std::unique_ptr<long[]> distances;  // millimeters
    };

    void PopulateSensorProperties(const Json::Value& config);
    void ProcessSensorData();

    whisker::proto::LidarSensorProperties sensor_properties;
    whisker::OverwritingBuffer<Observation> observation_buffer;
    whisker::SensorTimeSync<std::chrono::milliseconds> sensor_time_sync;
    const std::unique_ptr<UrgSensor> urg;
    std::thread sensor_thread;
    std::atomic_bool run_sensor_thread = true;
};

#endif  // WHISKER_HOKUYO_H
