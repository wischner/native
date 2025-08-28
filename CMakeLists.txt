cmake_minimum_required(VERSION 3.16)
project(native LANGUAGES C CXX)

# Global C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- one knob: <platform>-<toolkit> ----
set(variant "" CACHE STRING [[
  build variant (format: <platform>-<toolkit>)
  Examples:
    linux-x11
    linux-motif
    linux-sdl2
    macos-cocoa
    gnustep-gnustep
    haiku-beapi
    windows-win32
]])
if(NOT variant OR variant STREQUAL "")
  set(variant "linux-x11")
endif()
string(TOLOWER "${variant}" variant)
message(STATUS "native: variant=${variant}")

# Derive platform/toolkit for subdirs
string(REPLACE "-" ";" _parts "${variant}")
list(LENGTH _parts _n)
if(NOT _n EQUAL 2)
  message(FATAL_ERROR "variant must be <platform>-<toolkit>, got: ${variant}")
endif()
list(GET _parts 0 platform)
list(GET _parts 1 toolkit)
set(platform "${platform}" CACHE INTERNAL "derived platform from variant")
set(toolkit  "${toolkit}"  CACHE INTERNAL "derived toolkit from variant")

# ------------------------------------------------------------------
# Docker targets live here (host side). Enabled by default so `make`
# does something useful even on a clean host without deps installed.
# ------------------------------------------------------------------
option(enable_docker "Add Docker helper targets" ON)
if(enable_docker)
  add_subdirectory(docker)
endif()

# ------------------------------------------------------------------
# Build the code only when appropriate:
#  - On the HOST: skip src/ if we're using docker (enable_docker=ON).
#  - INSIDE the CONTAINER: docker passes -Dbuild_inside_docker=ON,
#    so we include src/ and actually compile there.
#  - If you want a pure host build, configure with -Denable_docker=OFF.
# ------------------------------------------------------------------
option(build_inside_docker "internal: set by docker runner" OFF)
if(NOT enable_docker OR build_inside_docker)
  add_subdirectory(src)
  add_subdirectory(examples)
  # add_subdirectory(docs)
endif()
