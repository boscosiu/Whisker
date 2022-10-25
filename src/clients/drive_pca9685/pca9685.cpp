#include "pca9685.h"
#include <fcntl.h>
#include <unistd.h>
#include <linux/i2c-dev.h>
#include <sys/ioctl.h>
#include <glog/logging.h>

constexpr unsigned char channel_register_base = 0x06;

Pca9685::Pca9685(const Json::Value& config) {
    const auto i2c_device = config["i2c_device"].asString();
    const auto address = config["address"].asString();

    LOG(INFO) << "Connecting to PCA9685 with I2C device " << i2c_device << " at address " << address;

    i2c_fd = open(i2c_device.c_str(), O_RDWR);
    CHECK_ERR(i2c_fd) << "Error opening I2C device";
    CHECK_ERR(ioctl(i2c_fd, I2C_SLAVE, std::stoi(address, nullptr, 0))) << "Error configuring I2C device";

    // initialize the controller
    unsigned char buf[2];
    buf[0] = 0x00;    // select MODE1 configuration register
    buf[1] = 1 << 4;  // disable oscillator (required for setting pwm frequency)
    CHECK_ERR(write(i2c_fd, buf, 2));
    buf[0] = 0xfe;  // select pwm prescaler register
    buf[1] = 0x03;  // set pwm frequency to 1526 Hz (maximum supported)
    CHECK_ERR(write(i2c_fd, buf, 2));
    buf[0] = 0x00;    // select MODE1 configuration register
    buf[1] = 1 << 5;  // enable oscillator and enable auto-increment programming mode
    CHECK_ERR(write(i2c_fd, buf, 2));

    const auto& left_motor_channels = config["left_motor_channels"];
    left_motor.input_1_register_num = channel_register_base + (4 * left_motor_channels["input_1"].asUInt());
    left_motor.input_2_register_num = channel_register_base + (4 * left_motor_channels["input_2"].asUInt());
    left_motor.pwm_register_num = channel_register_base + (4 * left_motor_channels["pwm"].asUInt());

    const auto& right_motor_channels = config["right_motor_channels"];
    right_motor.input_1_register_num = channel_register_base + (4 * right_motor_channels["input_1"].asUInt());
    right_motor.input_2_register_num = channel_register_base + (4 * right_motor_channels["input_2"].asUInt());
    right_motor.pwm_register_num = channel_register_base + (4 * right_motor_channels["pwm"].asUInt());

    use_pwm_channel = config["use_pwm_channel"].asBool();
    throttle_scale_straight = config["throttle_scale_straight"].asFloat();
    throttle_scale_rotation = config["throttle_scale_rotation"].asFloat();

    // engage motor brake on startup
    MoveForward(0);
}

Pca9685::~Pca9685() {
    MoveForward(0);  // engage motor brake on shutdown/restart
    close(i2c_fd);
}

void Pca9685::MoveForward(float throttle) {
    throttle *= throttle_scale_straight;
    SetMotorThrottle(left_motor, throttle);
    SetMotorThrottle(right_motor, throttle);
}

void Pca9685::RotateRight(float throttle) {
    throttle *= throttle_scale_rotation;
    SetMotorThrottle(left_motor, throttle);
    SetMotorThrottle(right_motor, -throttle);
}

void Pca9685::SetMotorThrottle(const MotorInfo& motor_info, float throttle) {
    bool is_forward = true;

    if (throttle < 0) {
        throttle = -throttle;
        is_forward = false;
    }
    if (throttle > 1) {
        throttle = 1;
    }

    if (use_pwm_channel) {
        if (is_forward) {
            // forward or brake (input_1 = low, input_2 = high, pwm = pwm)
            WriteChannelValue(motor_info.input_1_register_num, 0);
            WriteChannelValue(motor_info.input_2_register_num, 1);
        } else {
            // reverse (input_1 = high, input_2 = low, pwm = pwm)
            WriteChannelValue(motor_info.input_1_register_num, 1);
            WriteChannelValue(motor_info.input_2_register_num, 0);
        }
        WriteChannelValue(motor_info.pwm_register_num, throttle);
    } else {
        if (throttle == 0) {
            // brake (input_1 = high, input_2 = high, pwm = unused)
            WriteChannelValue(motor_info.input_1_register_num, 1);
            WriteChannelValue(motor_info.input_2_register_num, 1);
        } else if (is_forward) {
            // forward (input_1 = inverse pwm, input_2 = high, pwm = unused)
            WriteChannelValue(motor_info.input_1_register_num, 1 - throttle);
            WriteChannelValue(motor_info.input_2_register_num, 1);
        } else {
            // reverse (input_1 = high, input_2 = inverse pwm, pwm = unused)
            WriteChannelValue(motor_info.input_1_register_num, 1);
            WriteChannelValue(motor_info.input_2_register_num, 1 - throttle);
        }
    }
}

void Pca9685::WriteChannelValue(unsigned char register_num, float value) {
    unsigned short on = 0;
    unsigned short off = 0;

    if (value >= 1) {
        on = 0x1000;
    } else if (value <= 0) {
        off = 0x1000;
    } else {
        off = 4095 * value;  // 4095 = 12-bit pwm
    }

    unsigned char buf[5];
    buf[0] = register_num;
    buf[1] = on & 0xff;
    buf[2] = on >> 8;
    buf[3] = off & 0xff;
    buf[4] = off >> 8;
    CHECK_ERR(write(i2c_fd, buf, 5));
}
