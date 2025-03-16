cmake_minimum_required(VERSION 3.16)
project(native LANGUAGES CXX)

# Set the default C++ standard for everything
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add subdirectories
add_subdirectory(src)
add_subdirectory(examples)