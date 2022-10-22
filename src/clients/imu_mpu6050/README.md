## Sensor Client for MPU-6050 Series IMUs

`whisker_client_imu_mpu6050`

This component obtains 6DOF (accelerometer/gyroscope) data from [InvenSense MPU-6000/6050](https://invensense.tdk.com/products/motion-tracking/6-axis/mpu-6050) IMUs.

[Datasheet](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Datasheet1.pdf)\
[Register Listing](https://invensense.tdk.com/wp-content/uploads/2015/02/MPU-6000-Register-Map1.pdf)

### Supported Platforms

Linux (GCC, Clang)

### Command Line Options

- `--config=[config file path]`
  - Use JSON configuration file at the given path (default: `config.json` in current directory)
- `--id=[client ID]`
  - Specify the vehicle-wide unique ID used to identify this component to the server (default: `imu0`)

### Configuration

This example config is available at [/config/config.json](../../../config/config.json).  Note the use of client ID as the configuration object's key.

```json5
// config.json
{
  "vehicle": {
    // ...
    "clients": {
      "imu0": {
        "i2c_device": "/dev/i2c-1",
        "address": "0x68",
        "zero_offsets": {
          "accel_x": 0,
          "accel_y": 0,
          "accel_z": 0,
          "gyro_x": 0,
          "gyro_y": 0,
          "gyro_z": 0
        },
        "position": {
          "roll": 0,
          "pitch": 0,
          "yaw": 0
        }
      }
    }
  }
}
```

| Key            | Type   |                                                                                                         |
|----------------|--------|---------------------------------------------------------------------------------------------------------|
| `i2c_device`   | string | Device node of system's I2C device                                                                      |
| `address`      | string | I2C address of sensor                                                                                   |
| `zero_offsets` | object | Offsets to apply to the accelerometer and gyroscope readings, in m/s<sup>2</sup> and rad/s <sup>1</sup> |
| `position`     | object | Orientation of the IMU in relation to the vehicle, in degrees                                           |

<sup>1</sup> One way to obtain these values is to record a series of `ObservationMessage`s with a motionless sensor then take the average and negate.  The goal is to have all values close to zero with only gravity acceleration being measured.

### Sensor Position

⚠ IMUs must be placed at the rotational axis of the vehicle for accurate tracking.  If the vehicle is differentially driven, this would be the center of the vehicle between the wheels.
