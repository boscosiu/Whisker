# Create a temporary CMake project to build a dependency as an external project so that find_package()
# can locate it at configuration time
function(whisker_build_cmake_dependency_project NAME URL SHA256_HASH CONFIG_ARGS)
    option(WHISKER_BUILD_DEPENDENCY_${NAME} "Build ${NAME}" ON)
    if(NOT WHISKER_BUILD_DEPENDENCY_${NAME})
        message("Using externally provided ${NAME}")
    else()
        message("Building ${NAME}")
        set(DEP_BUILD_DIR ${WHISKER_DEPENDENCIES_DIR}/${NAME})
        if(NOT (SHA256_HASH STREQUAL ""))
            set(DEP_HASH_FILE ${DEP_BUILD_DIR}/${SHA256_HASH})
            if(NOT EXISTS ${DEP_HASH_FILE})
                message("Performing clean build because dependency doesn't exist or its hash differs")
                file(REMOVE_RECURSE ${DEP_BUILD_DIR})
                file(WRITE ${DEP_HASH_FILE} "")
            endif()
        endif()
        file(WRITE ${DEP_BUILD_DIR}/CMakeLists.txt "
            cmake_minimum_required(VERSION 3.15)
            project(BuildDependency)
            include(ExternalProject)
            ExternalProject_Add(${NAME}
                PREFIX \"${DEP_BUILD_DIR}\"
                URL ${URL}
                URL_HASH SHA256=${SHA256_HASH}
                CMAKE_CACHE_ARGS
                    -DCMAKE_BUILD_TYPE:STRING=Release
                    -DCMAKE_CXX_STANDARD:STRING=${CMAKE_CXX_STANDARD}
                    -DCMAKE_CXX_STANDARD_REQUIRED:STRING=${CMAKE_CXX_STANDARD_REQUIRED}
                    -DCMAKE_CXX_EXTENSIONS:STRING=${CMAKE_CXX_EXTENSIONS}
                    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE:STRING=${CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE}
                    -DCMAKE_POLICY_DEFAULT_CMP0069:STRING=NEW
                    -DCMAKE_POLICY_DEFAULT_CMP0090:STRING=NEW
                    \"-DCMAKE_C_FLAGS_INIT:STRING=${WHISKER_COMPILER_FLAGS}\"
                    \"-DCMAKE_CXX_FLAGS_INIT:STRING=${WHISKER_COMPILER_FLAGS}\"
                    \"-DCMAKE_TOOLCHAIN_FILE:STRING=${CMAKE_TOOLCHAIN_FILE}\"
                    \"-DCMAKE_PREFIX_PATH:STRING=${CMAKE_PREFIX_PATH}\"
                    \"-DCMAKE_INSTALL_PREFIX:STRING=${DEP_BUILD_DIR}\"
                    ${CONFIG_ARGS}
                ${ARGN}
            )
        ")
        execute_process(
            COMMAND ${CMAKE_COMMAND} -G ${CMAKE_GENERATOR} .
            WORKING_DIRECTORY ${DEP_BUILD_DIR}
        )
        execute_process(
            COMMAND ${CMAKE_COMMAND} --build . --config Release --parallel ${WHISKER_DEPENDENCY_JOBS}
            WORKING_DIRECTORY ${DEP_BUILD_DIR}
            RESULT_VARIABLE EXIT_CODE
        )
        if(EXIT_CODE)
            message(FATAL_ERROR "Error building ${NAME}")
        endif()
        set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} ${DEP_BUILD_DIR} PARENT_SCOPE)
    endif()
endfunction()

# This is where we download, build, and 'install' the dependencies
set(WHISKER_DEPENDENCIES_DIR ${CMAKE_CURRENT_BINARY_DIR}/whisker_dependencies)

# WHISKER_DEPENDENCY_NUM_JOBS is a configurable option where 0 means to use the system's processor count
set(WHISKER_DEPENDENCY_JOBS ${WHISKER_DEPENDENCY_NUM_JOBS})
if(WHISKER_DEPENDENCY_JOBS EQUAL "0")
    include(ProcessorCount)
    ProcessorCount(WHISKER_DEPENDENCY_JOBS)
endif()
message("Using ${WHISKER_DEPENDENCY_JOBS} parallel jobs to build dependencies")

whisker_build_cmake_dependency_project(ZLIB
    https://github.com/zlib-ng/zlib-ng/archive/refs/tags/2.0.6.tar.gz
    8258b75a72303b661a238047cb348203d88d9dddf85d480ed885f375916fcab6
    "-DZLIB_COMPAT:STRING=ON -DZLIB_ENABLE_TESTS:STRING=OFF -DBUILD_SHARED_LIBS:STRING=OFF -DSKIP_INSTALL_FILES:STRING=ON"
)

whisker_build_cmake_dependency_project(jsoncpp
    https://github.com/open-source-parsers/jsoncpp/archive/refs/tags/1.9.5.tar.gz
    f409856e5920c18d0c2fb85276e24ee607d2a09b5e7d5f0a371368903c275da2
    "-DBUILD_SHARED_LIBS:STRING=OFF -DJSONCPP_WITH_TESTS:STRING=OFF -DJSONCPP_WITH_POST_BUILD_UNITTEST:STRING=OFF"
)

whisker_build_cmake_dependency_project(gflags
    https://github.com/gflags/gflags/archive/v2.2.2.tar.gz
    34af2f15cf7367513b352bdcd2493ab14ce43692d2dcd9dfc499492966c64dcf
    "-DREGISTER_INSTALL_PREFIX:STRING=OFF"
)

whisker_build_cmake_dependency_project(glog
    https://github.com/google/glog/archive/refs/tags/v0.6.0.tar.gz
    8a83bf982f37bb70825df71a9709fa90ea9f4447fb3c099e1d720a439d88bad6
    "-DBUILD_SHARED_LIBS:STRING=OFF -DWITH_GTEST:STRING=OFF -DWITH_UNWIND:STRING=OFF -DBUILD_TESTING:STRING=OFF"
)

whisker_build_cmake_dependency_project(protobuf
    https://github.com/protocolbuffers/protobuf/releases/download/v21.5/protobuf-cpp-3.21.5.tar.gz
    58c8a18b4ec22655535c493155c5465a8903e8249094ceead87e00763bdbc44f
    "-DBUILD_SHARED_LIBS:STRING=OFF
     -Dprotobuf_BUILD_TESTS:STRING=OFF
     -Dprotobuf_MSVC_STATIC_RUNTIME:STRING=OFF"
)

whisker_build_cmake_dependency_project(libwebsockets
    https://github.com/warmcat/libwebsockets/archive/refs/tags/v4.3.2.tar.gz
    6a85a1bccf25acc7e8e5383e4934c9b32a102880d1e4c37c70b27ae2a42406e1
    "-DLWS_WITH_SSL:STRING=OFF
     -DLWS_WITH_SHARED:STRING=OFF
     -DLWS_WITH_ZIP_FOPS:STRING=ON
     -DLWS_WITH_ZLIB:STRING=ON
     -DLWS_WITH_BUNDLED_ZLIB:STRING=OFF
     -DLWS_WITHOUT_CLIENT:STRING=ON
     -DLWS_WITHOUT_TESTAPPS:STRING=ON
     -DLWS_WITHOUT_TEST_SERVER:STRING=ON
     -DLWS_WITHOUT_TEST_PING:STRING=ON
     -DLWS_WITHOUT_TEST_CLIENT:STRING=ON
     -DLWS_WITH_MINIMAL_EXAMPLES:STRING=OFF"
)

whisker_build_cmake_dependency_project(ZeroMQ
    https://github.com/zeromq/libzmq/archive/ee09926cbd089cec780896cf7aa65f8bfd4b1b86.zip
    60d76d2bd171900733c32f3c8ea5e4ec50b8b6e16c93a8554e4c6ac2c2875122
    "-DENABLE_DRAFTS:STRING=ON
     -DENABLE_WS:STRING=OFF
     -DWITH_LIBBSD:STRING=OFF
     -DWITH_LIBSODIUM:STRING=OFF
     -DWITH_DOC:STRING=OFF
     -DWITH_DOCS:STRING=OFF
     -DBUILD_SHARED:STRING=OFF
     -DBUILD_TESTS:STRING=OFF
     -DENABLE_CPACK:STRING=OFF"
)

if(NOT WHISKER_CLIENT_ONLY_BUILD)
    whisker_build_cmake_dependency_project(PNG
        https://download.sourceforge.net/libpng/libpng-1.6.37.tar.xz
        505e70834d35383537b6491e7ae8641f1a4bed1876dbfe361201fc80868d88ca
        "-DPNG_SHARED:STRING=OFF -DPNG_TESTS:STRING=OFF"
    )

    whisker_build_cmake_dependency_project(Eigen3
        https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.bz2
        b4c198460eba6f28d34894e3a5710998818515104d6e74e5cc331ce31e46e626
        "-DBUILD_TESTING:STRING=OFF -DEIGEN_BUILD_DOC:STRING=OFF"
    )

    whisker_build_cmake_dependency_project(Ceres
        https://github.com/ceres-solver/ceres-solver/archive/refs/tags/2.1.0.tar.gz
        ccbd716a93f65d4cb017e3090ae78809e02f5426dce16d0ee2b4f8a4ba2411a8
        "-DBUILD_TESTING:STRING=OFF
         -DBUILD_EXAMPLES:STRING=OFF
         -DBUILD_BENCHMARKS:STRING=OFF"
    )

    whisker_build_cmake_dependency_project(Lua
        https://github.com/LuaDist/lua/archive/5.3.2.tar.gz
        fd91904ca1025481acbc0effd578cc2b9994d359a86d84ac6959ecb56f65a749
        "-DBUILD_SHARED_LIBS:STRING=OFF -DLUA_USE_DLOPEN:STRING=OFF -DLUA_USE_READLINE:STRING=OFF"
    )

    whisker_build_cmake_dependency_project(absl
        https://github.com/abseil/abseil-cpp/archive/refs/tags/20220623.0.tar.gz
        4208129b49006089ba1d6710845a45e31c59b0ab6bff9e5788a87f55c5abd602
        ""
    )

    whisker_build_cmake_dependency_project(cartographer
        https://github.com/boscosiu/cartographer-whiskerdev/archive/refs/tags/r1.tar.gz
        91e20609ef1143923737d0e3e0b33ea60d829ae63fcbc25a9464fd2f9d62face
        "-DCARTOGRAPHER_ENABLE_TESTING:STRING=OFF -DCARTOGRAPHER_BUILD_UTILITIES:STRING=OFF -DCARTOGRAPHER_ENABLE_CAIRO_USAGE:STRING=OFF"
    )
endif()
