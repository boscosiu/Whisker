## Capability Client for PCA9685 PWM Controllers

`whisker_client_drive_pca9685`

This component provides a set of capabilities for driving differentially steered vehicles.  It does so by interfacing with an [NXP PCA9685](https://www.nxp.com/products/power-management/lighting-driver-and-controller-ics/led-controllers/16-channel-12-bit-pwm-fm-plus-ic-bus-led-controller:PCA9685) controller to output PWM signals to a pair of motor drivers.

[Datasheet](https://www.nxp.com/docs/en/data-sheet/PCA9685.pdf)

### Supported Platforms

Linux (GCC, Clang)

### Command Line Options

- `-config [config file path]`
    - Use JSON configuration file at the given path (default: `config.json` in current directory)
- `-id [client ID]`
    - Specify the vehicle-wide unique ID used to identify this component to the server (default: `drive0`)

### Configuration

This example config is available at [/config/config.json](../../../config/config.json).  Note the use of client ID as the configuration object's key.

```json5
// config.json
{
  "vehicle": {
    // ...
    "clients": {
      "drive0": {
        "i2c_device": "/dev/i2c-1",
        "address": "0x40",
        "left_motor_channels": {
          "input_1": 0,
          "input_2": 1,
          "pwm": 2
        },
        "right_motor_channels": {
          "input_1": 3,
          "input_2": 4,
          "pwm": 5
        },
        "throttle_scale_straight": 1.0,
        "throttle_scale_rotation": 1.0
      }
    }
  }
}
```

| Key                       | Type   |                                                                                   |
|---------------------------|--------|-----------------------------------------------------------------------------------|
| `i2c_device`              | string | Device node of system's I2C device                                                |
| `address`                 | string | I2C address of PCA9685 controller                                                 |
| `left_motor_channels`     | object | PCA9685 channels that are connected to the logic inputs of the left motor driver  |
| `right_motor_channels`    | object | PCA9685 channels that are connected to the logic inputs of the right motor driver |
| `throttle_scale_straight` | number | Multiply throttle values for forward/reverse commands with this number            |
| `throttle_scale_rotation` | number | Multiply throttle values for rotation commands with this number                   |

### Capabilities

The capabilities registered for the vehicle are `forward`, `reverse`, `left`, `right`, and `stop`.  The input for each capability (except `stop`) is the throttle value to apply to the operation and should be a number between 0 and 1.

### Motor Driver Logic

When a motor needs to be driven forward, the channel designated as `input_1` is set low and `input_2` is set high.  For reverse, `input_1` is set high and `input_2` is set low.  In both cases, the channel designated as `pwm` is driven with a duty cycle proportional to the throttle value.  Stopping a motor is the same as driving forward with zero throttle.

|         | `input_1` | `input_2` | `pwm` |
|---------|:---------:|:---------:|:-----:|
| Forward |     L     |     H     |  PWM  |
| Reverse |     H     |     L     |  PWM  |
| Stop    |     L     |     H     |   L   |

If the motor driver uses a single pin for direction, one of the `input_n` entries can be assigned an unused channel and left disconnected.
