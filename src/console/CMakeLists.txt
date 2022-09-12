message("Processing ${CMAKE_CURRENT_LIST_FILE}")

add_custom_target(whisker_console ALL
    COMMAND npm install
    COMMAND npm run build
    DEPENDS whisker_proto_console_js
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_command(TARGET whisker_console POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E tar cv ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/whisker_console.zip --format=zip .
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build
)
