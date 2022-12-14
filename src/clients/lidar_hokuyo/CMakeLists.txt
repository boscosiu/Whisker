find_package(gflags REQUIRED)
find_package(glog REQUIRED)
find_package(jsoncpp REQUIRED)

include(ExternalProject)
ExternalProject_Add(urg_library
    PREFIX urg_library
    URL https://github.com/UrgNetwork/urg_library/archive/refs/tags/ver.1.2.7.tar.gz
    URL_HASH SHA256=f82c21b9aea2330f932a14ae7ee43a0760e784c88190beabb4659680425aa7a3
    PATCH_COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/PatchUrgUtilsC.cmake
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    EXCLUDE_FROM_ALL TRUE
)
set(URG_LIBRARY_INCLUDE_DIR
    ${CMAKE_CURRENT_BINARY_DIR}/urg_library/src/urg_library/current/include/c
)
set(URG_LIBRARY_SOURCE_FILES
    ${CMAKE_CURRENT_BINARY_DIR}/urg_library/src/urg_library/current/src/urg_connection.c
    ${CMAKE_CURRENT_BINARY_DIR}/urg_library/src/urg_library/current/src/urg_ring_buffer.c
    ${CMAKE_CURRENT_BINARY_DIR}/urg_library/src/urg_library/current/src/urg_sensor.c
    ${CMAKE_CURRENT_BINARY_DIR}/urg_library/src/urg_library/current/src/urg_serial.c
    ${CMAKE_CURRENT_BINARY_DIR}/urg_library/src/urg_library/current/src/urg_tcpclient.c
    ${CMAKE_CURRENT_BINARY_DIR}/urg_library/src/urg_library/current/src/urg_utils.c
)
set_source_files_properties(${URG_LIBRARY_SOURCE_FILES} PROPERTIES GENERATED TRUE)

add_executable(whisker_client_lidar_hokuyo
    main.cpp
    hokuyo.cpp
    ${URG_LIBRARY_SOURCE_FILES}
)
target_include_directories(whisker_client_lidar_hokuyo
    PRIVATE ${URG_LIBRARY_INCLUDE_DIR}
)
target_link_libraries(whisker_client_lidar_hokuyo
    whisker_core
    whisker_proto_client
    gflags
    glog::glog
    jsoncpp_static
)
add_dependencies(whisker_client_lidar_hokuyo
    urg_library
)
