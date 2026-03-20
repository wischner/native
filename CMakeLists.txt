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
    set(DOCKER_IMAGE_X11 "wischner/gcc-x86_64-linux-x11:${DOCKER_TAG}" CACHE STRING "Docker image for X11 builds")
    set(DOCKER_IMAGE_SDL2 "wischner/gcc-x86_64-linux-sdl:${DOCKER_TAG}" CACHE STRING "Docker image for SDL2 builds")

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

        function(add_linux_docker_build_target target_name toolkit build_dir image)
            add_custom_target(${target_name}
                COMMAND ${DOCKER_EXECUTABLE} run --rm
                    -u ${HOST_UID}:${HOST_GID}
                    -v ${CMAKE_SOURCE_DIR}:${CMAKE_SOURCE_DIR}
                    -w ${CMAKE_SOURCE_DIR}
                    ${image}
                    cmake -S ${CMAKE_SOURCE_DIR} -B ${CMAKE_SOURCE_DIR}/${build_dir}
                    -DTOOLKIT=${toolkit}
                    -DCMAKE_BUILD_TYPE=$<IF:$<BOOL:${CMAKE_BUILD_TYPE}>,${CMAKE_BUILD_TYPE},Debug>
                COMMAND ${DOCKER_EXECUTABLE} run --rm
                    -u ${HOST_UID}:${HOST_GID}
                    -v ${CMAKE_SOURCE_DIR}:${CMAKE_SOURCE_DIR}
                    -w ${CMAKE_SOURCE_DIR}
                    ${image}
                    cmake --build ${CMAKE_SOURCE_DIR}/${build_dir}
                USES_TERMINAL
                COMMENT "Building ${toolkit} with Docker image ${image}"
            )
        endfunction()

        add_linux_docker_build_target(docker-x11 X11 build/linux-x11 ${DOCKER_IMAGE_X11})
        add_linux_docker_build_target(docker-sdl2 SDL2 build/linux-sdl2 ${DOCKER_IMAGE_SDL2})
    endif()
endif()

# Add subdirectories
add_subdirectory(src)
add_subdirectory(examples)
