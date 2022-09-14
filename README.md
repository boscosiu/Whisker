# 🐱 Whisker

Copyright Bosco Siu, Qadyence Inc. (<opensource@qadyence.com>)

## Overview

Whisker is a distributed localization, mapping, and control system for autonomous ground vehicles.  It uses a client-server model to offload the computational tasks of a fleet of vehicles to a central server.  This simplifies multi-agent coordination and reduces the cost, complexity, and power requirements of the vehicles.

## Building

To build Whisker you need to have a C++17 compiler and CMake >= 3.15 installed.  All required libraries will be downloaded and built inside the build directory during initial CMake configuration.

Use the CMake CLI to generate the config (or set the options in your project if your IDE has CMake support):

```
mkdir build
cmake -B build [OPTIONS]
```

Where `[OPTIONS]` may be zero or more of the following:

- `-DCMAKE_BUILD_TYPE=Release` (💡 highly recommended)
  - Sets the CMake build type.  If left unset the toolchain default is used which is usually `Debug`, a configuration that is useful for debugging but unusable in production.
- `-DWHISKER_CLIENT_ONLY_BUILD=ON`
  - Tell CMake to generate a configuration to only build clients.  This omits the building of some dependencies if you don't need the console, server, or utilities.  Default is `OFF` which means to generate a configuration to build everything.
- `-DWHISKER_OPTIMIZATION_LTO=ON`
  - Enable link time optimization if the `Release` build type is chosen.  Default is `OFF`.
- `-DWHISKER_OPTIMIZATION_ARCH=native`
  - Pass `-march=native` to the compiler, telling it to optimize code for your system's processor.  The provided value will be appended unmodified to `-march=`.  No flag is passed if this option is unset or if your compiler doesn't understand `-march`.
- `-DWHISKER_DEPENDENCY_NUM_JOBS=8`
  - Set the number of concurrent jobs to use for building dependencies.  Default is `0` which means to use the number of CPUs detected in your system.
- `-DWHISKER_BUILD_DEPENDENCY_<dependency>=OFF` (⚠️ not recommended)
  - Tell CMake to use the `<dependency>` library that's installed on the system instead of building and linking to our own version.  If left unset all dependencies will be built (i.e., we won't link to anything on the system).
- `-DWHISKER_PROTOC_PATH=/path/to/protoc`
  - Manually specify the location of the Protobuf compiler executable.  Default is to use the `protoc` that is created as part of building Protobuf.  This should be set if you are cross compiling since the binary that is cross built likely won't run on this system.

CMake will build the dependencies into `build/whisker_dependencies` and create targets for the various components.  Targets can be built with `make` (e.g., `make whisker_server`) or through your CMake-supporting IDE.  Build output will be placed in `build/whisker_bin`.

Components will read their configuration from a `config.json` in the current directory by default.  The documentation for each component outlines what they expect to see in this file.  The sample config at [config/config.json](config/config.json) can be used as a starting point.

## Components

- `whisker_server`
  - Central server component
  - [src/server/README.md](src/server/README.md)
- `whisker_console`
  - Browser-based console application
  - [src/console/README.md](src/console/README.md)
- `whisker_client_lidar_hokuyo`
  - Sensor client for Hokuyo lidars
  - [src/clients/lidar_hokuyo/README.md](src/clients/lidar_hokuyo/README.md)
- `whisker_client_lidar_sick`
  - Sensor client for Sick lidars
  - [src/clients/lidar_sick/README.md](src/clients/lidar_sick/README.md)
- `whisker_client_imu_mpu6050`
  - Sensor client for InvenSense MPU-6050 series IMUs
  - [src/clients/imu_mpu6050/README.md](src/clients/imu_mpu6050/README.md)
- `whisker_client_drive_pca9685`
  - Capability client for controlling differential drive motors with the NXP PCA9685 PWM controller
  - [src/clients/drive_pca9685/README.md](src/clients/drive_pca9685/README.md)
- `whisker_client_observation_playback`
  - Client that pretends to be a sensor client by replaying a previously recorded observation log
  - [src/clients/observation_playback/README.md](src/clients/observation_playback/README.md)
- `whisker_util_convert_rosbag`
  - Utility to convert a ROS bag file containing IMU or lidar messages to a Whisker observation log
  - [src/utils/convert_rosbag/README.md](src/utils/convert_rosbag/README.md)

## License

Whisker is licensed under the terms of the Apache License, Version 2.0.  See [LICENSE](LICENSE) for more information.
