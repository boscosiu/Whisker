#include "hokuyo.h"
#include <cmath>
#include <glog/logging.h>
#include <urg_errno.h>
#include <urg_sensor.h>
#include <urg_utils.h>

class UrgSensor : public urg_t {};

Hokuyo::Hokuyo(const Json::Value& config) : urg(std::make_unique<UrgSensor>()) {
    const auto is_serial_device = config["is_serial_device"].asBool();
    const auto serial_device_or_ip = config["serial_device_or_ip"].asString();

    if (is_serial_device) {
        LOG(INFO) << "Connecting to sensor with serial device " << serial_device_or_ip;
        CHECK_EQ(urg_open(urg.get(), URG_SERIAL, serial_device_or_ip.c_str(), 115200), URG_NO_ERROR)
                << "Error connecting to sensor";
    } else {
        LOG(INFO) << "Connecting to sensor at IP " << serial_device_or_ip;
        CHECK_EQ(urg_open(urg.get(), URG_ETHERNET, serial_device_or_ip.c_str(), 10940), URG_NO_ERROR)
                << "Error connecting to sensor";
    }

    PopulateSensorProperties(config);

    CHECK_EQ(urg_laser_on(urg.get()), URG_NO_ERROR);
    sensor_thread = std::thread{&Hokuyo::ProcessSensorData, this};
}

Hokuyo::~Hokuyo() {
    run_sensor_thread = false;
    sensor_thread.join();
    CHECK_EQ(urg_laser_off(urg.get()), URG_NO_ERROR);
    urg_close(urg.get());
}

whisker::proto::LidarSensorProperties Hokuyo::GetSensorProperties() const {
    return sensor_properties;
}

whisker::proto::ObservationMessage Hokuyo::GetLatestObservation() {
    whisker::proto::ObservationMessage observation_message;

    observation_buffer.Read([&observation_message](const auto buffered_observation) {
        observation_message.set_timestamp(buffered_observation->timestamp);

        const auto lidar_observation = observation_message.mutable_lidar_observation();
        lidar_observation->mutable_measurements()->Reserve(buffered_observation->num_distances);
        for (auto i = 0; i < buffered_observation->num_distances; ++i) {
            lidar_observation->add_measurements(buffered_observation->distances[i]);
        }
    });

    return observation_message;
}

void Hokuyo::PopulateSensorProperties(const Json::Value& config) {
    int min_step, max_step;
    urg_step_min_max(urg.get(), &min_step, &max_step);

    sensor_properties.set_starting_angle(urg_step2rad(urg.get(), min_step));
    sensor_properties.set_angular_resolution(urg_step2rad(urg.get(), 1) - urg_step2rad(urg.get(), 0));
    sensor_properties.set_rotations_per_second(1'000'000 / urg_scan_usec(urg.get()));

    const auto position = sensor_properties.mutable_position();
    position->set_x(config["position"]["x"].asDouble());
    position->set_y(config["position"]["y"].asDouble());
    position->set_r(config["position"]["r"].asDouble() * M_PI / 180);
}

void Hokuyo::ProcessSensorData() {
    CHECK_EQ(urg_start_measurement(urg.get(), URG_DISTANCE, URG_SCAN_INFINITY, 0), URG_NO_ERROR);

    while (run_sensor_thread) {
        observation_buffer.Write([this](const auto observation) {
            if (!observation->distances) {
                observation->distances = std::make_unique<long[]>(urg_max_data_size(urg.get()));
            }

            long timestamp;
            observation->num_distances = urg_get_distance(urg.get(), observation->distances.get(), &timestamp);
            CHECK_GE(observation->num_distances, 0) << "Error getting distance data from sensor";

            observation->timestamp = sensor_time_sync.GetAdjustedTime(timestamp);
        });
    }

    CHECK_EQ(urg_stop_measurement(urg.get()), URG_NO_ERROR);
}
