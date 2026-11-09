# Build a link probe and package its resolved SDK, never application assets.
if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
    message(FATAL_ERROR "SDK packaging requires CMAKE_BUILD_TYPE=Release")
endif()
if(NOT NATIVE_SDK_VERSION MATCHES "^v[0-9]+\\.[0-9]+\\.[0-9]+$" OR
   NOT NATIVE_SDK_ID MATCHES "^[a-z0-9][a-z0-9_-]+$")
    message(FATAL_ERROR "Set a numeric NATIVE_SDK_VERSION tag and NATIVE_SDK_ID")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)

# The file API gives the packager the actual, ordered, transitive link line.
file(WRITE "${CMAKE_BINARY_DIR}/.cmake/api/v1/query/codemodel-v2" "")
add_executable(native_sdk_probe "${PROJECT_SOURCE_DIR}/tests/native_sdk_probe.cpp")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_libraries(native_sdk_probe PRIVATE
        -Wl,--whole-archive native -Wl,--no-whole-archive)
else()
    target_link_libraries(native_sdk_probe PRIVATE native)
    target_link_options(native_sdk_probe PRIVATE
        "LINKER:-force_load,$<TARGET_FILE:native>")
endif()

configure_file("${CMAKE_CURRENT_LIST_DIR}/sdk-info.json.in"
    "${CMAKE_BINARY_DIR}/sdk-info.json" @ONLY)
add_custom_target(native_sdk_package ALL
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/package_sdk.py"
        --build "${CMAKE_BINARY_DIR}"
    DEPENDS native_sdk_probe
    USES_TERMINAL
    VERBATIM)
