# Set the system name
set(CMAKE_SYSTEM_NAME Windows)

# Specify the target architecture (32-bit or 64-bit)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Set the MinGW compilers
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)

# Specify where the MinGW headers and libraries are located
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

# Ensure CMake uses the MinGW system for finding libraries
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
