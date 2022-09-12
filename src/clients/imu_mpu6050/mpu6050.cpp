#include "mpu6050.h"
#include <cmath>
#include <fcntl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <glog/logging.h>

constexpr unsigned char readings_register_base = 0x3b;

Mpu6050::Mpu6050(const Json::Value& config) {
    const auto i2c_device = config["i2c_device"].asString();
    const auto address = config["address"].asString();

    LOG(INFO) << "Connecting to MPU-6050 with I2C device " << i2c_device << " at address " << address;

    i2c_fd = open(i2c_device.c_str(), O_RDWR);
    CHECK_ERR(i2c_fd) << "Error opening I2C device";
    CHECK_ERR(ioctl(i2c_fd, I2C_SLAVE, std::stoi(address, nullptr, 0))) << "Error configuring I2C device";

    const auto& offsets = config["zero_offsets"];
    offset_accel_x = offsets["accel_x"].asDouble();
    offset_accel_y = offsets["accel_y"].asDouble();
    offset_accel_z = offsets["accel_z"].asDouble();
    offset_gyro_x = offsets["gyro_x"].asDouble();
    offset_gyro_y = offsets["gyro_y"].asDouble();
    offset_gyro_z = offsets["gyro_z"].asDouble();

    PopulateSensorProperties(config);

    unsigned char buf[2];
    buf[0] = 0x6b;  // select PWR_MGMT_1 configuration register
    buf[1] = 1;     // disable sleep, use gyroscope clock reference
    CHECK_ERR(write(i2c_fd, buf, 2));

    sensor_thread = std::thread{&Mpu6050::ProcessSensorData, this};
}

Mpu6050::~Mpu6050() {
    run_sensor_thread = false;
    sensor_thread.join();

    unsigned char buf[2];
    buf[0] = 0x6b;    // select PWR_MGMT_1 configuration register
    buf[1] = 1 << 6;  // enable sleep
    CHECK_ERR(write(i2c_fd, buf, 2));

    close(i2c_fd);
}

whisker::proto::ImuSensorProperties Mpu6050::GetSensorProperties() const {
    return sensor_properties;
}

whisker::proto::ObservationMessage Mpu6050::GetLatestObservation() {
    whisker::proto::ObservationMessage observation_message;

    observation_buffer.Read([this, &observation_message](const auto buffered_observation) {
        observation_message.set_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                  buffered_observation->timestamp.time_since_epoch())
                                                  .count());

        const auto imu_observation = observation_message.mutable_imu_observation();
        short value;

        value = (buffered_observation->readings[0] << 8) | buffered_observation->readings[1];
        imu_observation->set_linear_acceleration_x((value * 9.81 / 16384) + offset_accel_x);

        value = (buffered_observation->readings[2] << 8) | buffered_observation->readings[3];
        imu_observation->set_linear_acceleration_y((value * 9.81 / 16384) + offset_accel_y);

        value = (buffered_observation->readings[4] << 8) | buffered_observation->readings[5];
        imu_observation->set_linear_acceleration_z((value * 9.81 / 16384) + offset_accel_z);

        value = (buffered_observation->readings[8] << 8) | buffered_observation->readings[9];
        imu_observation->set_angular_velocity_x((value * M_PI / 180 / 131) + offset_gyro_x);

        value = (buffered_observation->readings[10] << 8) | buffered_observation->readings[11];
        imu_observation->set_angular_velocity_y((value * M_PI / 180 / 131) + offset_gyro_y);

        value = (buffered_observation->readings[12] << 8) | buffered_observation->readings[13];
        imu_observation->set_angular_velocity_z((value * M_PI / 180 / 131) + offset_gyro_z);
    });

    return observation_message;
}

void Mpu6050::PopulateSensorProperties(const Json::Value& config) {
    sensor_properties.set_roll(config["position"]["roll"].asDouble() * M_PI / 180);
    sensor_properties.set_pitch(config["position"]["pitch"].asDouble() * M_PI / 180);
    sensor_properties.set_yaw(config["position"]["yaw"].asDouble() * M_PI / 180);
}

void Mpu6050::ProcessSensorData() {
    while (run_sensor_thread) {
        observation_buffer.Write([this](const auto observation) {
            CHECK_ERR(write(i2c_fd, &readings_register_base, 1));
            CHECK_ERR(read(i2c_fd, observation->readings, sizeof(observation->readings)));
            observation->timestamp = std::chrono::system_clock::now();
        });
    }
}
