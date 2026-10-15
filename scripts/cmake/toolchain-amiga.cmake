# AmigaOS 3.1/m68k compiler settings for the runtime qualification checks.
# A passing check is a prerequisite for adding a Native UI backend.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR m68k)
set(NATIVE_AMIGA_ROOT /opt/amiga CACHE PATH "Amiga compiler prefix")
set(NATIVE_AMIGA_CRT nix20 CACHE STRING "Amiga C runtime")
find_program(CMAKE_CXX_COMPILER m68k-amigaos-g++
    HINTS "${NATIVE_AMIGA_ROOT}/bin" REQUIRED)
find_program(CMAKE_AR m68k-amigaos-ar
    HINTS "${NATIVE_AMIGA_ROOT}/bin" REQUIRED)
find_program(CMAKE_RANLIB m68k-amigaos-ranlib
    HINTS "${NATIVE_AMIGA_ROOT}/bin" REQUIRED)
set(CMAKE_CXX_FLAGS_INIT "-m68000 -mcrt=${NATIVE_AMIGA_CRT} -Os")
set(CMAKE_FIND_ROOT_PATH
    "${NATIVE_AMIGA_ROOT}" "${NATIVE_AMIGA_ROOT}/m68k-amigaos")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Forward selections into the nested configure used by try_compile.
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES
    NATIVE_AMIGA_ROOT NATIVE_AMIGA_CRT)
