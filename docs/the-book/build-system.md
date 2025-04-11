# Build system

This chapter describes how the **native** build system is organized using CMake. It explains how platforms and toolkits are conditionally compiled, how the shared code is isolated, and how to extend the system to support new operating systems or UI backends.

## Overview

The build is managed using **CMake**, with `make` as the intended build tool. The project is structured to:

- Automatically detect the current platform
- Allow explicit selection of a toolkit using the `TOOLKIT` variable
- Keep shared C++ code isolated in a single core library
- Cleanly separate platform and toolkit implementations

## Root configuration

The top-level `CMakeLists.txt` sets up the project, the C++ standard, and includes the core source and example directories:

```cmake
cmake_minimum_required(VERSION 3.16)
project(native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src)
add_subdirectory(examples)
```

## Core library setup

The core logic lives in `src/`. This directory contains all platform-independent C++ code, and conditionally includes the appropriate platform and toolkit subdirectories:

```cmake
add_library(native STATIC)

target_include_directories(native
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(native PRIVATE
    geometry.cpp
    pen.cpp
    screen.cpp
    wnd.cpp
    app_wnd.cpp
    app.cpp
)

add_subdirectory(platforms)

if(NOT WIN32 AND NOT HAIKU)
    add_subdirectory(toolkits)
endif()
```

The shared logic in this directory is entirely portable and reused by all platforms and toolkits.

## Platform handling

Platform-specific backends are located in `src/platforms/`, one subdirectory per OS. The appropriate platform is selected automatically using CMake variables like `WIN32`, `APPLE`, and `UNIX`.

The logic for selecting the platform is in `src/platforms/CMakeLists.txt`:

```cmake
if(WIN32)
    add_subdirectory(windows)
elseif(UNIX AND NOT APPLE)
    add_subdirectory(linux)
elseif(HAIKU)
    # add_subdirectory(haiku)
endif()
```

Each platform directory contributes its own source files and links to system libraries as needed. For example, `src/platforms/windows/CMakeLists.txt`:

```cmake
target_sources(native PRIVATE
    main.cpp
    screen.cpp
)

target_link_libraries(native PRIVATE
    user32
    gdi32
    shell32
)
```

macOS and Haiku will be enabled when their ports are complete.

## Toolkit selection

Toolkit backends are located in `src/toolkits/`. Only one toolkit is compiled at a time. The selected toolkit is controlled by the `TOOLKIT` variable, which is defined and evaluated in `src/toolkits/CMakeLists.txt`:

```cmake
set(TOOLKIT "X11" CACHE STRING "Toolkit backend to use (SDL2, OpenMotif, etc.)")

if(TOOLKIT STREQUAL "SDL2")
    add_subdirectory(sdl2)
elseif(TOOLKIT STREQUAL "OPENMOTIF")
    add_subdirectory(openmotif)
elseif(TOOLKIT STREQUAL "X11")
    add_subdirectory(x11)
else()
    message(FATAL_ERROR "Unknown toolkit: ${TOOLKIT}")
endif()
```

Each toolkit directory contributes its own drawing, event handling, and possibly font rendering code. Dependencies such as SDL2 or Motif are located using `find_package()` inside the toolkit's own `CMakeLists.txt`.

Toolkits are only compiled for platforms that require them. For example, Windows and Haiku do not use a separate toolkit and are handled entirely in their platform backends.

## Header structure

The main public header is located at:

```
include/native.h
```

This header exposes all shared C++ interfaces, including geometry types, window classes, the drawing system (`gpx`), and application control. It includes no platform-specific or toolkit-specific declarations.

## Examples

Each example lives in its own subdirectory under `examples/`. The examples are compiled separately and link against the `native` library:

```
examples/
├── app_example/
│   └── CMakeLists.txt
│   └── main.cpp
```

An example `CMakeLists.txt`:

```cmake
add_executable(app-example main.cpp)
target_link_libraries(app-example PRIVATE native)
```

All examples are included from the top-level `CMakeLists.txt`:

```cmake
add_subdirectory(examples)
```

## Adding a new platform

To add support for a new platform:

1. Create a new subdirectory under `src/platforms/` (e.g. `solaris/`)
2. Add the necessary source files and `CMakeLists.txt`
3. Update `src/platforms/CMakeLists.txt` to detect the platform and include the directory

Example:

```cmake
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "SunOS")
    add_subdirectory(solaris)
```

## Adding a new toolkit

To add a new toolkit:

1. Create a new subdirectory under `src/toolkits/` (e.g. `openlook/`)
2. Add drawing, input, and system integration logic in C++
3. Add a `CMakeLists.txt` that contributes source files and links any dependencies
4. Add the toolkit to the conditionals in `src/toolkits/CMakeLists.txt`:

```cmake
elseif(TOOLKIT STREQUAL "OPENLOOK")
    add_subdirectory(openlook)
```

## Summary

- The `native` build is modular, clean, and organized by platform and toolkit.
- The `TOOLKIT` variable selects a single backend for UI drawing and input.
- Platform code is selected automatically and provides system integration.
- All shared C++ logic lives in `src/` and is declared in `include/native.h`.
- Adding support for a new target involves only adding a subdirectory and updating a few CMake conditionals.

This structure makes it easy to port the library to new systems and test backends in isolation.
