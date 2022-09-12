find_package(gflags REQUIRED)
find_package(glog REQUIRED)
find_package(jsoncpp REQUIRED)

add_executable(whisker_client_imu_mpu6050
    main.cpp
    mpu6050.cpp
)
target_link_libraries(whisker_client_imu_mpu6050
    whisker_core
    whisker_proto_client
    gflags
    glog::glog
    jsoncpp_static
)
