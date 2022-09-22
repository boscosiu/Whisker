## Dependencies

Whisker uses the following libraries.  They are downloaded and built by CMake during initial configuration.

| Library       | License                                 | Used In Component             | Source                                                |
|---------------|-----------------------------------------|-------------------------------|-------------------------------------------------------|
| gflags        | `BSD-3-Clause`                          | All                           | <https://github.com/gflags/gflags>                    |
| glog          | `BSD-3-Clause`                          | All                           | <https://github.com/google/glog>                      |
| JsonCpp       | `MIT`                                   | All                           | <https://github.com/open-source-parsers/jsoncpp>      |
| libwebsockets | `MIT`                                   | All                           | <https://github.com/warmcat/libwebsockets>            |
| protobuf      | `BSD-3-Clause`                          | All                           | <https://github.com/protocolbuffers/protobuf>         |
| ZeroMQ        | `LGPL-3.0-or-later` + linking exception | All                           | <https://github.com/zeromq/libzmq>                    |
| zlib-ng       | `Zlib`                                  | All                           | <https://github.com/zlib-ng/zlib-ng>                  |
| Abseil        | `Apache-2.0`                            | `whisker_server`              | <https://github.com/abseil/abseil-cpp>                |
| Cartographer  | `Apache-2.0`                            | `whisker_server`              | <https://github.com/boscosiu/cartographer-whiskerdev> |
| Ceres Solver  | `BSD-3-Clause`                          | `whisker_server`              | <https://github.com/ceres-solver/ceres-solver>        |
| Eigen         | `MPL-2.0`                               | `whisker_server`              | <https://gitlab.com/libeigen/eigen>                   |
| libpng        | `libpng-2.0`                            | `whisker_server`              | <https://sourceforge.net/p/libpng/code>               |
| Lua           | `MIT`                                   | `whisker_server`              | <https://github.com/LuaDist/lua>                      |
| URG Library   | `BSD-2-Clause`                          | `whisker_client_lidar_hokuyo` | <https://github.com/UrgNetworks/urg_library>          |
