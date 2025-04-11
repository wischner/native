# Getting Started

This chapter explains how to build the **native** UI library and run the included examples. It also introduces how platforms and toolkits are organized in the project structure.

## Prerequisites

To build **native**, you will need:

- A C++20-compatible compiler (e.g. `g++`, `clang++`, or MSVC)
- CMake version 3.16 or newer
- Standard build tools (e.g. `make`)
- Development packages for any required toolkit (e.g. X11, SDL2, OpenMotif)

## Cloning the repository

```bash
git clone https://github.com/tstih/native.git
cd native
```

## Building the library

To configure and build the project:

```bash
mkdir build
cd build
cmake ..
make
```

You can optionally select a specific toolkit backend:

```bash
cmake -DTOOLKIT=X11 ..
```

If not specified, the default toolkit is `"X11"`.

## Platform and toolkit structure

The `src/` directory contains **pure, platform-independent C++** code that is shared by all platforms and toolkits. This includes logic for geometry, drawing, window management, and the main application class.

The file `native.h` serves as the **main public header**. It declares all shared types and interfaces used throughout the library, such as `pen`, `rect`, `point`, `wnd`, `gpx`, and `img`.

### Platform code

Each supported operating system has its own platform implementation located in:

```
src/platforms/windows/     # Windows platform backend
src/platforms/haiku/       # Haiku platform backend
src/platforms/apple/       # macOS (Cocoa) platform backend
src/platforms/linux/       # Linux platform backend
```

Platform selection is automatic and based on your system (e.g. `WIN32`, `HAIKU`, `APPLE`, etc.).

### Toolkit code

Some platforms (like Linux) also require a toolkit to handle windowing and drawing. Toolkit-specific code is organized as:

```
src/toolkits/x11/
src/toolkits/sdl2/
src/toolkits/openmotif/
```

The toolkit is selected at build time using the `TOOLKIT` CMake variable:

```bash
cmake -DTOOLKIT=SDL2 ..
```

Toolkit selection logic is defined in the top-level `CMakeLists.txt`:

```cmake
set(TOOLKIT "X11" CACHE STRING "Toolkit backend to use (SDL2, OpenMotif, etc.)")

if(TOOLKIT STREQUAL "SDL2")
    add_subdirectory(sdl2)
elseif(TOOLKIT STREQUAL "OpenMotif")
    add_subdirectory(openmotif)
elseif(TOOLKIT STREQUAL "X11")
    add_subdirectory(x11)
else()
    message(FATAL_ERROR "Unknown toolkit: ${TOOLKIT}")
endif()
```

Toolkit sources are only added for systems where they are required. For example, Windows and Haiku do not use a toolkit:

```cmake
if(NOT WIN32 AND NOT HAIKU)
    add_subdirectory(toolkits)
endif()
```

macOS will be added in the same way when its platform backend is complete.

## Running the examples

After building, you can run the example programs from the build directory:

```bash
./examples/example_hello
```

Make sure the chosen toolkit works correctly on your platform and that any dependencies (like X11 or SDL2) are installed.

## Folder structure overview

```
native/
├── src/                # Platform-independent core logic (pure C++)
│   ├── platforms/      # Platform-specific implementations (per OS)
│   └── toolkits/       # Toolkit-specific implementations (Linux only)
├── include/            # Public headers (including native.h)
├── examples/           # Sample applications
├── docs/books/         # This documentation
├── native.h            # (included from include/native.h)
└── CMakeLists.txt      # Top-level build configuration
```
