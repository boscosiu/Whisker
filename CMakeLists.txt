cmake_minimum_required(VERSION 3.15)

# Settings to control build configuration
option(WHISKER_CLIENT_ONLY_BUILD "Generate a smaller configuration to only build clients" OFF)
option(WHISKER_OPTIMIZATION_LTO "Enable link time optimization for Release config" OFF)
set(WHISKER_OPTIMIZATION_ARCH "" CACHE STRING "Architecture type to use for -march={type} compiler flag (empty/unset = generic, 'native' = this system's architecture)")
set(WHISKER_DEPENDENCY_NUM_JOBS "0" CACHE STRING "Number of parallel jobs to build dependencies (0 = use system processor count)")
# Additional config variables defined elsewhere:
# WHISKER_BUILD_DEPENDENCY_<dependency> - whether to build <dependency> or let find_package() try to locate it on the system
# WHISKER_PROTOC_PATH - location of the 'protoc' protobuf compiler to use (should be specified if cross compiling)

project(Whisker C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add compiler flags to be used when building dependencies and our own targets
function(whisker_add_global_compiler_flags FLAGS)
    include(CheckCXXCompilerFlag)
    check_cxx_compiler_flag(${FLAGS} WHISKER_FLAGS_SUPPORTED)
    if(WHISKER_FLAGS_SUPPORTED)
        message("Adding to compiler flags: ${FLAGS}")
        set(WHISKER_COMPILER_FLAGS "${WHISKER_COMPILER_FLAGS} ${FLAGS}" PARENT_SCOPE)
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${FLAGS}" PARENT_SCOPE)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${FLAGS}" PARENT_SCOPE)
    else()
        message(WARNING "Compiler doesn't support requested flags: ${FLAGS}")
    endif()
    unset(WHISKER_FLAGS_SUPPORTED CACHE)
endfunction()

if(WHISKER_OPTIMIZATION_LTO)
    message("Enabling link time optimization for Release config")
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()
if(NOT WHISKER_OPTIMIZATION_ARCH STREQUAL "")
    whisker_add_global_compiler_flags("-march=${WHISKER_OPTIMIZATION_ARCH}")
endif()

if(MSVC)
    # https://docs.microsoft.com/en-us/cpp/build/reference/zc-cplusplus
    whisker_add_global_compiler_flags("/Zc:__cplusplus")
    whisker_add_global_compiler_flags("/D_USE_MATH_DEFINES")
endif()

# Gather all of our output executables into a separate directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/whisker_bin)

# Prefer find_package() to use Config mode before Module mode
set(CMAKE_FIND_PACKAGE_PREFER_CONFIG TRUE)

include(BuildDependencies.cmake)

add_subdirectory(src/proto)

add_subdirectory(src/core)

add_subdirectory(src/clients)

if(NOT WHISKER_CLIENT_ONLY_BUILD)
    add_subdirectory(src/console)

    add_subdirectory(src/server)

    add_subdirectory(src/utils)
endif()
