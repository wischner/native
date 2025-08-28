# Cross-compile to Haiku (x86_64) with the Haiku cross toolchain
# These triplets are provided by the haiku/cross-compiler image.

set(CMAKE_SYSTEM_NAME Haiku)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Compilers from the toolchain
set(CMAKE_C_COMPILER   x86_64-unknown-haiku-gcc)
set(CMAKE_CXX_COMPILER x86_64-unknown-haiku-g++)
# Haiku doesn't use a Windows-style resource compiler here; omit CMAKE_RC_COMPILER.

# Where to find target sysroot
# (Provided by the toolchain; CMake will find headers/libs via the compiler)
# set(CMAKE_SYSROOT /system/develop)  # usually not required explicitly

# Search behavior: prefer target for libs/includes, host for programs
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
