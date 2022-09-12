find_package(gflags REQUIRED)
find_package(glog REQUIRED)

# ROS dependencies
find_package(rosbag_storage QUIET)
find_package(sensor_msgs QUIET)

if(rosbag_storage_FOUND AND sensor_msgs_FOUND)
    add_executable(whisker_util_convert_rosbag
        main.cpp
    )
    target_include_directories(whisker_util_convert_rosbag
        PRIVATE ${rosbag_storage_INCLUDE_DIRS}
        PRIVATE ${sensor_msgs_INCLUDE_DIRS}
    )
    target_link_libraries(whisker_util_convert_rosbag
        whisker_core
        whisker_proto_client
        gflags
        glog::glog
        ${rosbag_storage_LIBRARIES}
        ${sensor_msgs_LIBRARIES}
    )
endif()
