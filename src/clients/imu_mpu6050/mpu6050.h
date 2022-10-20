#ifndef WHISKER_MPU6050_H
#define WHISKER_MPU6050_H

#include <atomic>
#include <chrono>
#include <thread>
#include <json/json.h>
#include <whisker/overwriting_buffer.h>
#include <client.pb.h>

class Mpu6050 final {
  public:
    Mpu6050(const Json::Value& config);
    ~Mpu6050();

    Mpu6050(const Mpu6050&) = delete;
    Mpu6050& operator=(const Mpu6050&) = delete;

    whisker::proto::ImuSensorProperties GetSensorProperties() const;
    whisker::proto::ObservationMessage GetLatestObservation();

  private:
    struct Observation {
        std::chrono::system_clock::time_point timestamp;
        unsigned char readings[14];
    };

    void PopulateSensorProperties(const Json::Value& config);
    void ProcessSensorData();

    whisker::proto::ImuSensorProperties sensor_properties;
    whisker::OverwritingBuffer<Observation> observation_buffer;
    int i2c_fd;
    double offset_accel_x;
    double offset_accel_y;
    double offset_accel_z;
    double offset_gyro_x;
    double offset_gyro_y;
    double offset_gyro_z;
    std::thread sensor_thread;
    std::atomic_bool run_sensor_thread = true;
};

#endif  // WHISKER_MPU6050_H
