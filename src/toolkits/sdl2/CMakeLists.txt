# SDL2 Toolkit backend

message(STATUS "Configuring SDL2 Toolkit...")

# Optionally let users set their SDL2 installation path
if(NOT SDL2_DIR)
    message(STATUS "SDL2_DIR not set, relying on default CMake search paths.")
endif()

# Look for SDL2 (this should work as it relies on pkg-config or system installation)
find_package(SDL2 REQUIRED)

# If SDL2 is not found, provide a clear error
if(NOT SDL2_FOUND)
    message(FATAL_ERROR "SDL2 was not found! Please install SDL2 or specify the path using:
    -DSDL2_DIR=/path/to/SDL2
    You can build SDL2 from source here: https://github.com/libsdl-org/SDL")
endif()

message(STATUS "SDL2 found: ${SDL2_DIR}")

# Add your SDL2 sources to the 'native' target (assuming it's already defined in the parent CMakeLists.txt)
target_sources(native PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

# Include directory for SDL2 (handled by find_package)
target_include_directories(native PRIVATE
    ${SDL2_INCLUDE_DIRS}  # This is handled automatically by find_package(SDL2)
)

# Link against SDL2
target_link_libraries(native PRIVATE SDL2::SDL2)

message(STATUS "SDL2 Toolkit successfully configured!")
