# Runs one Docker configure/build transaction without overlapping another
# invocation for the same backend tree (for example, F5 and a terminal build).
#
# MIT License (see: LICENSE)
# Copyright (C) 2026 Tomaz Stih

foreach(required EXECUTABLE USER IMAGE SOURCE BUILD)
    if(NOT DEFINED NATIVE_DOCKER_${required})
        message(FATAL_ERROR "Missing NATIVE_DOCKER_${required}")
    endif()
endforeach()

file(MAKE_DIRECTORY "${NATIVE_DOCKER_BUILD}")
file(LOCK "${NATIVE_DOCKER_BUILD}/.docker-build.lock" GUARD PROCESS)

set(container_args run --rm
    -u "${NATIVE_DOCKER_USER}"
    -v "${NATIVE_DOCKER_SOURCE}:${NATIVE_DOCKER_SOURCE}"
    -w "${NATIVE_DOCKER_SOURCE}"
    "${NATIVE_DOCKER_IMAGE}")

execute_process(
    COMMAND "${NATIVE_DOCKER_EXECUTABLE}" ${container_args}
        cmake -S "${NATIVE_DOCKER_SOURCE}" -B "${NATIVE_DOCKER_BUILD}"
        ${NATIVE_DOCKER_CONFIGURE_ARGS}
    RESULT_VARIABLE configure_result)
if(NOT configure_result STREQUAL "0")
    message(FATAL_ERROR "Docker configure failed: ${configure_result}")
endif()

execute_process(
    COMMAND "${NATIVE_DOCKER_EXECUTABLE}" ${container_args}
        cmake --build "${NATIVE_DOCKER_BUILD}" ${NATIVE_DOCKER_BUILD_ARGS}
    RESULT_VARIABLE build_result)
if(NOT build_result STREQUAL "0")
    message(FATAL_ERROR "Docker build failed: ${build_result}")
endif()
