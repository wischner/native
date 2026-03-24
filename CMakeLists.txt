cmake_minimum_required(VERSION 3.16)
project(native LANGUAGES CXX)

# Set the default C++ standard for everything
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_program(OBJCXX_COMPILER NAMES g++ clang++)
set(CMAKE_OBJCXX_COMPILER ${OBJCXX_COMPILER})

# Detect if we are building on Haiku
if(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
    set(HAIKU TRUE)
endif()

if(UNIX AND NOT APPLE)
    find_program(DOCKER_EXECUTABLE docker)

    set(DOCKER_TAG "latest" CACHE STRING "Docker tag used for Linux toolkit build images")
    set(DOCKER_TAG_HAIKU "1.1.0" CACHE STRING "Docker tag used for the Haiku cross-build image")
    set(DOCKER_IMAGE_X11 "wischner/gcc-x86_64-linux-x11:${DOCKER_TAG}" CACHE STRING "Docker image for X11 builds")
    set(DOCKER_IMAGE_SDL2 "wischner/gcc-x86_64-linux-sdl:${DOCKER_TAG}" CACHE STRING "Docker image for SDL2 builds")
    set(DOCKER_IMAGE_OPENMOTIF "wischner/gcc-x86_64-linux-motif:${DOCKER_TAG}" CACHE STRING "Docker image for OpenMotif builds")
    set(DOCKER_IMAGE_GNUSTEP "wischner/gcc-x86_64-linux-gnustep:${DOCKER_TAG}" CACHE STRING "Docker image for GNUstep builds")
    set(DOCKER_IMAGE_WINDOWS_MINGW "wischner/gcc-x86_64-windows-mingw-w64:${DOCKER_TAG}" CACHE STRING "Docker image for Windows MinGW builds")
    set(DOCKER_IMAGE_HAIKU "wischner/gcc-x86_64-haiku:${DOCKER_TAG_HAIKU}" CACHE STRING "Docker image for Haiku cross-builds")
    set(DOCKER_IMAGE_OPENLOOK "wischner/gcc-x86_64-linux-openlook:${DOCKER_TAG}" CACHE STRING "Docker image for OpenLook-style Linux builds")

    if(DOCKER_EXECUTABLE)
        execute_process(
            COMMAND id -u
            OUTPUT_VARIABLE HOST_UID
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        execute_process(
            COMMAND id -g
            OUTPUT_VARIABLE HOST_GID
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )

        function(add_docker_cmake_build_target target_name build_dir image)
            set(cmake_args ${ARGN})
            set(has_explicit_build_type FALSE)

            foreach(cmake_arg IN LISTS cmake_args)
                if(cmake_arg MATCHES "^-DCMAKE_BUILD_TYPE=")
                    set(has_explicit_build_type TRUE)
                    break()
                endif()
            endforeach()

            if(NOT has_explicit_build_type)
                list(APPEND cmake_args
                    -DCMAKE_BUILD_TYPE=$<IF:$<BOOL:${CMAKE_BUILD_TYPE}>,${CMAKE_BUILD_TYPE},Debug>)
            endif()

            add_custom_target(${target_name}
                COMMAND ${DOCKER_EXECUTABLE} run --rm
                    -u ${HOST_UID}:${HOST_GID}
                    -v ${CMAKE_SOURCE_DIR}:${CMAKE_SOURCE_DIR}
                    -w ${CMAKE_SOURCE_DIR}
                    ${image}
                    cmake -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_SOURCE_DIR}/${build_dir}
                    ${cmake_args}
                COMMAND ${DOCKER_EXECUTABLE} run --rm
                    -u ${HOST_UID}:${HOST_GID}
                    -v ${CMAKE_SOURCE_DIR}:${CMAKE_SOURCE_DIR}
                    -w ${CMAKE_SOURCE_DIR}
                    ${image}
                    cmake --build ${CMAKE_SOURCE_DIR}/${build_dir}
                USES_TERMINAL
                COMMENT "Building ${target_name} with Docker image ${image}"
            )
        endfunction()

        add_docker_cmake_build_target(docker-x11 build/linux-x11 ${DOCKER_IMAGE_X11}
            -DTOOLKIT=X11)
        add_docker_cmake_build_target(docker-sdl2 build/linux-sdl2 ${DOCKER_IMAGE_SDL2}
            -DTOOLKIT=SDL2)
        add_docker_cmake_build_target(docker-openmotif build/linux-openmotif ${DOCKER_IMAGE_OPENMOTIF}
            -DTOOLKIT=MOTIF)
        add_docker_cmake_build_target(docker-openlook build/linux-openlook ${DOCKER_IMAGE_OPENLOOK}
            -DTOOLKIT=OPENLOOK)
        add_docker_cmake_build_target(docker-gnustep build/linux-gnustep ${DOCKER_IMAGE_GNUSTEP}
            -DTOOLKIT=GNUSTEP)
        add_docker_cmake_build_target(docker-win build/windows-mingw-w64 ${DOCKER_IMAGE_WINDOWS_MINGW}
            -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/cmake/toolchain-mingw.cmake)
        add_docker_cmake_build_target(docker-haiku build/haiku ${DOCKER_IMAGE_HAIKU}
            -DCMAKE_TOOLCHAIN_FILE=${CMAKE_SOURCE_DIR}/cmake/toolchain-haiku.cmake)
    endif()
endif()

# Add subdirectories
add_subdirectory(src)
add_subdirectory(examples)
