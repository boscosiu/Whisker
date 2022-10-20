## Sensor Client for Sick Lidars

`whisker_client_lidar_sick`

This component obtains range data from 2D lidars manufactured by [SICK AG](https://www.sick.com) using the CoLa A protocol.  It assumes the data packet format is produced from a lidar with factory default settings.

### Supported Platforms

Linux (GCC, Clang), macOS (Xcode Clang), Windows (MSVC)

### Command Line Options

- `-config [config file path]`
  - Use JSON configuration file at the given path (default: `config.json` in current directory)
- `-id [client ID]`
  - Specify the vehicle-wide unique ID used to identify this component to the server (default: `sick0`)

### Configuration

This example config is available at [/config/config.json](../../../config/config.json).  Note the use of client ID as the configuration object's key.

```json5
// config.json
{
  "vehicle": {
    // ...
    "clients": {
      "sick0": {
        "address": "192.168.0.1",
        "port": 2111,
        "rotates_clockwise": false,
        "angle_offset": 90,
        "position": {
          "x": 0,
          "y": 0,
          "r": 0
        }
      }
    }
  }
}
```

| Key                 | Type    |                                                                                 |
|---------------------|---------|---------------------------------------------------------------------------------|
| `address`           | string  | IP or hostname of lidar                                                         |
| `port`              | number  | Port on lidar that is using the CoLa A protocol                                 |
| `rotates_clockwise` | boolean | Whether the lidar is sweeping in a clockwise direction                          |
| `angle_offset`      | number  | Angle in degrees that the lidar considers to be the front of the sensor         |
| `x`                 | number  | X offset in meters of center of sensor away from center of vehicle <sup>1</sup> |
| `y`                 | number  | Y offset in meters of center of sensor away from center of vehicle <sup>1</sup> |
| `r`                 | number  | Rotation in degrees of front of sensor away from front of vehicle <sup>1</sup>  |

<sup>1</sup> Transforms in Whisker are expressed as (when viewed from above): +x towards front of vehicle, +y towards left of vehicle, +r counterclockwise with 0 at front of vehicle
