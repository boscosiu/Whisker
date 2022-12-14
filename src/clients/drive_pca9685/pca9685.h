#ifndef WHISKER_PCA9685_H
#define WHISKER_PCA9685_H

#include <json/json.h>

class Pca9685 final {
  public:
    Pca9685(const Json::Value& config);
    ~Pca9685();

    Pca9685(const Pca9685&) = delete;
    Pca9685& operator=(const Pca9685&) = delete;

    void MoveForward(float throttle);
    void RotateRight(float throttle);

  private:
    struct MotorInfo {
        unsigned char input_1_register_num;
        unsigned char input_2_register_num;
        unsigned char pwm_register_num;
    };

    void SetMotorThrottle(const MotorInfo& motor_info, float throttle);
    void WriteChannelValue(unsigned char register_num, float value);

    int i2c_fd;
    MotorInfo left_motor;
    MotorInfo right_motor;
    bool use_pwm_channel;
    float throttle_scale_straight;
    float throttle_scale_rotation;
};

#endif  // WHISKER_PCA9685_H
