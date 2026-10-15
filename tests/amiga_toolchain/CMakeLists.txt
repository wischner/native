cmake_minimum_required(VERSION 3.20)
project(native_amiga_toolchain_check LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Check both facilities even when one fails, preserving complete diagnostics.
try_compile(native_amiga_threads
    "${CMAKE_CURRENT_BINARY_DIR}/threads"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/thread_runtime.cpp"
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
    COMPILE_DEFINITIONS -Wall -Wextra -pedantic
    LINK_LIBRARIES pthread
    OUTPUT_VARIABLE native_amiga_thread_log)
try_compile(native_amiga_filesystem
    "${CMAKE_CURRENT_BINARY_DIR}/filesystem"
    SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/filesystem_runtime.cpp"
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
    COMPILE_DEFINITIONS -Wall -Wextra -pedantic
    OUTPUT_VARIABLE native_amiga_filesystem_log)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/threads.log"
    "${native_amiga_thread_log}")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/filesystem.log"
    "${native_amiga_filesystem_log}")
message(STATUS "Amiga C++20 threading link: ${native_amiga_threads}")
message(STATUS "Amiga C++20 filesystem link: ${native_amiga_filesystem}")
if(NOT native_amiga_threads OR NOT native_amiga_filesystem)
    message(FATAL_ERROR
        "Amiga C++20 runtime is incomplete. See threads.log and filesystem.log. "
        "No Amiga Native library or Vision executable has been built.")
endif()

# Building these targets retains executable probes for a target-machine run.
# Cross-link success alone does not validate scheduling or filesystem behavior.
add_executable(native_amiga_thread_probe thread_runtime.cpp)
target_link_libraries(native_amiga_thread_probe PRIVATE pthread)
add_executable(native_amiga_filesystem_probe filesystem_runtime.cpp)
