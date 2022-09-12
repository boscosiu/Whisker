message("Processing ${CMAKE_CURRENT_LIST_FILE}")

find_package(Protobuf REQUIRED)

if(NOT DEFINED WHISKER_PROTOC_PATH)
    if(CMAKE_CROSSCOMPILING)
        # The cross compiled protoc is probably incompatible with the host machine, so emit a warning if a
        # custom protoc location wasn't provided
        message(WARNING "Cross compiling but WHISKER_PROTOC_PATH not set")
    endif()
    get_target_property(WHISKER_PROTOC_PATH protobuf::protoc LOCATION)
endif()

message("Using Protobuf compiler at ${WHISKER_PROTOC_PATH}")

# Create a CMake target to generate C++ code for a proto file
#
# TARGET_NAME = name to give the created target
# PROTO_FILE = proto file to compile
# additional args = proto lib dependencies to link with
function(whisker_add_proto_target TARGET_NAME PROTO_FILE)
    get_filename_component(PROTO_FILE ${PROTO_FILE} ABSOLUTE)
    get_filename_component(PROTO_BASE_NAME ${PROTO_FILE} NAME_WE)
    get_filename_component(PROTO_DIR ${PROTO_FILE} DIRECTORY)
    set(OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/whisker_proto/${PROTO_BASE_NAME})
    set(OUTPUT_H ${OUTPUT_DIR}/${PROTO_BASE_NAME}.pb.h)
    set(OUTPUT_CC ${OUTPUT_DIR}/${PROTO_BASE_NAME}.pb.cc)

    add_custom_command(
        OUTPUT ${OUTPUT_H} ${OUTPUT_CC}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${OUTPUT_DIR}
        COMMAND ${WHISKER_PROTOC_PATH} --cpp_out=${OUTPUT_DIR} --proto_path=${PROTO_DIR} ${PROTO_FILE}
        DEPENDS ${WHISKER_PROTOC_PATH} ${PROTO_FILE}
    )

    add_library(${TARGET_NAME}
        ${OUTPUT_CC}
    )
    target_include_directories(${TARGET_NAME}
        INTERFACE ${OUTPUT_DIR}
    )
    target_link_libraries(${TARGET_NAME}
        PUBLIC protobuf::libprotobuf-lite
        PUBLIC ${ARGN}
    )
endfunction()

whisker_add_proto_target(whisker_proto_common
    common.proto
)

whisker_add_proto_target(whisker_proto_client
    client.proto
    whisker_proto_common
)

if(NOT WHISKER_CLIENT_ONLY_BUILD)
    whisker_add_proto_target(whisker_proto_console
        console.proto
        whisker_proto_common
    )

    # The JavaScript console can use proto files directly, so just make them available in 'public'
    add_custom_target(whisker_proto_console_js
        COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/common.proto ${CMAKE_CURRENT_SOURCE_DIR}/console.proto
            ${CMAKE_CURRENT_SOURCE_DIR}/../console/public
    )
endif()
