find_package(gflags REQUIRED)
find_package(glog REQUIRED)
find_package(jsoncpp REQUIRED)

add_executable(whisker_client_drive_pca9685
    main.cpp
    pca9685.cpp
)
target_link_libraries(whisker_client_drive_pca9685
    whisker_core
    whisker_proto_client
    gflags
    glog::glog
    jsoncpp_static
)
