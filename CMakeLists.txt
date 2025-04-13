cmake_minimum_required(VERSION 3.16)
project(native LANGUAGES CXX)

# Set the default C++ standard for everything
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Detect if we are building on Haiku
if(CMAKE_SYSTEM_NAME STREQUAL "Haiku")
    set(HAIKU TRUE)
endif()

# Add subdirectories
add_subdirectory(src)
add_subdirectory(examples)