# SDL3 Toolkit backend

message(STATUS "Configuring SDL3 Toolkit...")

# Optionally let users set their SDL3 installation path
if(NOT SDL3_DIR)
    message(STATUS "SDL3_DIR not set, relying on default CMake search paths.")
endif()

# Look for SDL3 (requires SDL3Config.cmake)
find_package(SDL3 QUIET CONFIG)

# If SDL3 is not found, provide a clear error
if(NOT SDL3_FOUND)
    message(FATAL_ERROR "SDL3 was not found!
Please install SDL3 or specify the path using:
    -DSDL3_DIR=/path/to/SDL3
You can build SDL3 from source here: https://github.com/libsdl-org/SDL")
endif()

message(STATUS "SDL3 found: ${SDL3_DIR}")

# Add your SDL3 sources to the 'native' target (assuming it's already defined in the parent CMakeLists.txt)
target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

# Include directory for SDL3 (optional, handled by SDL3::SDL3)
target_include_directories(native PRIVATE
    ${SDL3_INCLUDE_DIRS}  # Not strictly needed with target_link_libraries
)

# Link against SDL3
target_link_libraries(native PRIVATE SDL3::SDL3)

message(STATUS "SDL3 Toolkit successfully configured!")
