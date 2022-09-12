message("Processing ${CMAKE_CURRENT_LIST_FILE}")

find_package(cartographer REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(glog REQUIRED)
find_package(jsoncpp REQUIRED)
find_package(PNG REQUIRED)

add_executable(whisker_server
    main.cpp
    cartographer_map.cpp
)
target_link_libraries(whisker_server
    whisker_core
    whisker_proto_client
    whisker_proto_console
    cartographer
    Eigen3::Eigen
    glog::glog
    jsoncpp_static
    PNG::PNG
)

# The Lua configuration files in the Cartographer installation directory are
# needed at runtime, so to keep our deployment portable we retain a copy
add_custom_command(TARGET whisker_server
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E remove_directory
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cartographer_base_config
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CARTOGRAPHER_CMAKE_DIR}/../configuration_files
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/cartographer_base_config
)
