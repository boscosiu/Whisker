## Sensor Client for Hokuyo Lidars

`whisker_client_lidar_hokuyo`

This component obtains range data from 2D lidars manufactured by [Hokuyo](https://www.hokuyo-aut.jp).  It uses the company's [URG Library](https://github.com/UrgNetwork/urg_library) to communicate with sensors over Ethernet or serial port.

### Supported Platforms

Linux (GCC, Clang), macOS (Xcode Clang), Windows (MSVC)

### Command Line Options

- `-config [config file path]`
    - Use JSON configuration file at the given path (default: `config.json` in current directory)
- `-id [client ID]`
    - Specify the vehicle-wide unique ID used to identify this component to the server (default: `hokuyo0`)

### Configuration

This example config is available at [/config/config.json](../../../config/config.json).  Note the use of client ID as the configuration object's key.

```json5
// config.json
{
  "vehicle": {
    // ...
    "clients": {
      "hokuyo0": {
        "serial_device_or_ip": "/dev/ttyACM0",
        "is_serial_device": true,
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

| Key                   | Type    |                                                                                 |
|-----------------------|---------|---------------------------------------------------------------------------------|
| `serial_device_or_ip` | string  | Lidar's IP address or serial port device/name                                   |
| `is_serial_device`    | boolean | Whether to interpret the above value as a serial port instead of IP             |
| `x`                   | number  | X offset in meters of center of sensor away from center of vehicle <sup>1</sup> |
| `y`                   | number  | Y offset in meters of center of sensor away from center of vehicle <sup>1</sup> |
| `r`                   | number  | Rotation in degrees of front of sensor away from front of vehicle <sup>1</sup>  |

<sup>1</sup> Transforms in Whisker are expressed as (when viewed from above): +x towards front of vehicle, +y towards left of vehicle, +r counterclockwise with 0 at front of vehicle
