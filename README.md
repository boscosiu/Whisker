# 🐱 Whisker

Copyright Bosco Siu, Qadyence Inc. (<opensource@qadyence.com>)

## Overview

Whisker is a distributed localization, mapping, and control system for autonomous ground vehicles.  It uses a client-server model to offload the computational tasks of a fleet of vehicles to a central server.  This simplifies multi-agent coordination and reduces the cost, complexity, and power requirements of the vehicles.

The [Cartographer](https://github.com/cartographer-project/cartographer) system is used as the server's SLAM backend.

A **component** in a Whisker deployment is some software that takes on the role of a **client**, **server**, or **console**:
- A **client** is a component associated with a vehicle that produces sensor data from the vehicle (*sensor client*) or performs a task on the vehicle (*capability client*).  This means that on a vehicle with multiple sensors and actuators, a number of sensor and capability clients would be running simultaneously.
- The **server** is the central component that clients and consoles connect to.  It receives data from sensor clients to build maps and disseminates this information to consoles.  It also receives commands from consoles and routes them to capability clients to execute.
- A **console** is a component that makes use of map data (e.g., for visualization) and may send commands to capability clients.

## Components

The following components are included in this repo.  Please refer to component-specific docs for details on configuration and usage.

- `whisker_server`
  - Central server component
  - [Docs](src/server/README.md)
- `whisker_console`
  - Browser-based console application
  - [Docs](src/console/README.md)
- `whisker_client_lidar_hokuyo`
  - Sensor client for Hokuyo lidars
  - [Docs](src/clients/lidar_hokuyo/README.md)
- `whisker_client_lidar_sick`
  - Sensor client for Sick lidars
  - [Docs](src/clients/lidar_sick/README.md)
- `whisker_client_imu_mpu6050`
  - Sensor client for InvenSense MPU-6050 series IMUs
  - [Docs](src/clients/imu_mpu6050/README.md)
- `whisker_client_drive_pca9685`
  - Capability client for controlling differential drive motors with the NXP PCA9685 PWM controller
  - [Docs](src/clients/drive_pca9685/README.md)
- `whisker_client_observation_playback`
  - Client that pretends to be a sensor client by replaying a previously recorded observation log
  - [Docs](src/clients/observation_playback/README.md)
- `whisker_util_convert_rosbag`
  - Utility to convert a ROS 1 bag file containing IMU or lidar messages to a Whisker observation log
  - [Docs](src/utils/convert_rosbag/README.md)

### Supported Platforms

| Component                             | Linux (GCC, Clang) | macOS (Xcode Clang) | Windows (MSVC) |
|---------------------------------------|:------------------:|:-------------------:|:--------------:|
| `whisker_server`                      |         ✓          |          ✓          |       ✓        |
| `whisker_client_lidar_hokuyo`         |         ✓          |          ✓          |       ✓        |
| `whisker_client_lidar_sick`           |         ✓          |          ✓          |       ✓        |
| `whisker_client_imu_mpu6050`          |         ✓          |                     |                |
| `whisker_client_drive_pca9685`        |         ✓          |                     |                |
| `whisker_client_observation_playback` |         ✓          |          ✓          |       ✓        |
| `whisker_util_convert_rosbag`         |         ✓          |                     |       ✓        |

`whisker_console` runs in any browser that supports WebGL.

## Build

To build Whisker you need a C++17 compiler and [CMake](https://cmake.org) >= 3.15.  [Node.js](https://nodejs.org) is required to build the browser-based `whisker_console`.  All other dependencies will be downloaded and built by CMake during initial configuration.

Use the CMake CLI to generate the config (or set the options in your IDE if it has CMake support):

```
mkdir build ; cd build
cmake -S .. [OPTIONS]
```

Where `[OPTIONS]` may be zero or more of the following:

- `-DCMAKE_BUILD_TYPE=Release` (💡 recommended)
  - Sets the CMake build type.  If left unset the toolchain default is used which is usually `Debug`, a configuration that is useful for debugging but unusable in production.
- `-DWHISKER_CLIENT_ONLY_BUILD=ON`
  - Tell CMake to generate a configuration to only build clients.  This omits the building of some dependencies if you don't need the console, server, or utilities.  Default is `OFF` which means to generate a config to build everything.
- `-DWHISKER_OPTIMIZATION_LTO=ON`
  - Enable link time optimization if the `Release` build type is chosen.  Default is `OFF`.
- `-DWHISKER_OPTIMIZATION_ARCH=native`
  - Pass the given value as a `-march=` compiler flag to optimize code for an architecture.  For GCC and Clang, `native` means to optimize code for your system's processor.  No flag is passed if this option is unset or if your compiler doesn't understand `-march`.
- `-DWHISKER_DEPENDENCY_NUM_JOBS=8`
  - Set the number of concurrent jobs to use for building dependencies.  Default is `0` which means to use the number of CPUs detected in your system.
- `-DWHISKER_BUILD_DEPENDENCY_<dependency>=OFF`
  - Tell CMake to use the `<dependency>` library that's installed on the system instead of building and linking to our own version.  If left unset all dependencies will be built (i.e., we won't link to anything on the system).
- `-DWHISKER_PROTOC_PATH=/path/to/protoc`
  - Manually specify the location of the protobuf compiler executable.  Default is to use the `protoc` that is created as part of building protobuf.  This should be set if you are cross compiling since the binary that is cross built likely won't run on this system.

CMake will build the dependencies into `/build/whisker_dependencies` and create targets for the various components.

Components can then be built by CMake directly:

```
cmake --build . --parallel 8 --target [COMPONENTS]
```

Or through your IDE or native build tool (e.g., `make`, `ninja`), depending on the CMake generator used.

Build output will be placed in `/build/whisker_bin`.

## Configuration

Components obtain their configuration from a `config.json` file in the current directory by default.  The sample config at [/config/config.json](config/config.json) can be used as a starting point.

### Vehicles/Clients

Client configuration is provided by JSON objects inside `clients` in the shared vehicle settings.  The format of these objects is described in the [component-specific docs](#components).

While grouping client configs in a single file makes deployment easier, this is not a requirement.  Clients may use separate config files as long as clients on the same vehicle specify a common vehicle ID.

```json5
// config.json
{
  "vehicle": {
    "id": "vehicle0",
    "server_address": "tcp://localhost:9000",
    "keep_out_radius": 0.5,
    "clients": {
      "client ID x": {
        // ...
      },
      "client ID y": {
        // ...
      }
    }
  }
}
```

| Key               | Type   |                                                                                                                                               |
|-------------------|--------|-----------------------------------------------------------------------------------------------------------------------------------------------|
| `id`              | string | ID to give this vehicle                                                                                                                       |
| `server_address`  | string | ZeroMQ endpoint of the server that the clients should connect to                                                                              |
| `keep_out_radius` | number | Radius in meters of this vehicle including its furthest protrusion.  Taken by the navigation and visualization systems as the vehicle's size. |
| `clients`         | object | Config objects of the clients attached to this vehicle, keyed by client ID (specified in [component documentation](#components))              |

### Server

The server component uses a different configuration format.  See `whisker_server` [documentation](src/server/README.md#configuration) for details.

## Development

Communication between components is facilitated by [protobuf](https://developers.google.com/protocol-buffers), [ZeroMQ](https://zeromq.org), and [WebSocket](https://wikipedia.org/wiki/WebSocket).  These open source technologies are widely supported and have many language/platform implementations.

This section describes how to develop new clients and consoles.

### API

Protobuf messages are used exclusively in communications with the server.  The complete API is defined and documented in [/src/proto/client.proto](src/proto/client.proto) and [/src/proto/console.proto](src/proto/console.proto).

### Connections

The server processes client and console messages with separate handlers.  Each handler can accept ZeroMQ and WebSocket connections, configurable in the server's [configuration file](src/server/README.md#configuration).

The choice of ZeroMQ or WebSocket transport is up to the component.  ZeroMQ is somewhat more versatile and performant.  Browser-based components must use WebSocket.

ZeroMQ/C++ components can use the [factory](src/core/whisker/zmq_connection.h) function `whisker::ZmqConnection::CreateServerConnection(...)` to construct a connection abstraction.  WebSocket/JavaScript components can use the `Connection` [class](src/console/src/Connection.js).

For other transport/language combinations, components should implement the following:

#### ZeroMQ

- Use the `ZMQ_DEALER` socket type to connect to the server
- Set the `ZMQ_ROUTING_ID` option on the socket to be the client/console ID

#### WebSocket

- Use a URL of the form `ws://[server:port]/?client_id=[client/console ID]` to connect to the server
- Set `whisker` as the protocol string

#### Message Format

The buffer that is submitted to ZeroMQ or WebSocket should contain the message type followed by the serialized protobuf message.  For example, the buffer for an `ObservationMessage` would contain `"whisker.proto.ObservationMessage" + '\0' + [result of serializing an instance of ObservationMessage]`.

## License

Whisker is licensed under the terms of the Apache License, Version 2.0.  See [/LICENSE](LICENSE) for more information.

### Dependencies

This software's dependencies are not part of the distribution.  However, they are downloaded by CMake and statically linked into the output binaries by default.  Please be aware of your obligations under these licenses if you intend to redistribute.

| Dependency                                                          | License                                 | Used In Component             |
|---------------------------------------------------------------------|-----------------------------------------|-------------------------------|
| [gflags](https://github.com/gflags/gflags)                          | `BSD-3-Clause`                          | All                           |
| [glog](https://github.com/google/glog)                              | `BSD-3-Clause`                          | All                           |
| [JsonCpp](https://github.com/open-source-parsers/jsoncpp)           | `MIT`                                   | All                           |
| [libwebsockets](https://github.com/warmcat/libwebsockets)           | `MIT`                                   | All                           |
| [protobuf](https://github.com/protocolbuffers/protobuf)             | `BSD-3-Clause`                          | All                           |
| [ZeroMQ](https://github.com/zeromq/libzmq)                          | `LGPL-3.0-or-later` + linking exception | All                           |
| [zlib-ng](https://github.com/zlib-ng/zlib-ng)                       | `Zlib`                                  | All                           |
| [Abseil](https://github.com/abseil/abseil-cpp)                      | `Apache-2.0`                            | `whisker_server`              |
| [Cartographer](https://github.com/boscosiu/cartographer-whiskerdev) | `Apache-2.0`                            | `whisker_server`              |
| [Ceres Solver](https://github.com/ceres-solver/ceres-solver)        | `BSD-3-Clause`                          | `whisker_server`              |
| [Eigen](https://gitlab.com/libeigen/eigen)                          | `MPL-2.0`                               | `whisker_server`              |
| [libpng](https://sourceforge.net/p/libpng/code)                     | `libpng-2.0`                            | `whisker_server`              |
| [Lua](https://github.com/LuaDist/lua)                               | `MIT`                                   | `whisker_server`              |
| [URG Library](https://github.com/UrgNetwork/urg_library)            | `BSD-2-Clause`                          | `whisker_client_lidar_hokuyo` |
