message("Processing ${CMAKE_CURRENT_LIST_FILE}")

find_package(gflags REQUIRED)
find_package(glog REQUIRED)
find_package(jsoncpp REQUIRED)
find_package(libwebsockets REQUIRED)
find_package(Protobuf REQUIRED)
find_package(ZeroMQ REQUIRED)
find_package(ZLIB REQUIRED)

add_library(whisker_core
    whisker/init.cpp
    whisker/message_log.cpp
    whisker/task_queue.cpp
    whisker/websocket_connection.cpp
    whisker/zmq_connection.cpp
)
target_include_directories(whisker_core
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(whisker_core
    PRIVATE gflags
    PRIVATE glog::glog
    PRIVATE jsoncpp_static
    PRIVATE libzmq-static
    PRIVATE protobuf::libprotobuf-lite
    PRIVATE websockets
    PRIVATE ZLIB::ZLIB
)
