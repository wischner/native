# Build System

This chapter describes the build structure that exists in the repository today.
It focuses on the current CMake workflow and the separation between the host
control tree and the Linux toolkit build trees.

## Overview

The project uses CMake as its build entry point.

At the root, CMake does three things:

- sets the project language and C++ standard
- builds the `native` library and examples
- exposes Docker-backed Linux toolkit targets

The top-level build flow is:

```bash
cmake -S . -B out
cmake --build out --target docker-x11
cmake --build out --target docker-sdl2
```

## Build directories

The repository uses three different build directories for Linux work:

- `out/`
  - host-side CMake control tree
  - contains the generated top-level targets such as `docker-x11` and `docker-sdl2`

- `build/linux-x11/`
  - toolkit build tree for the Linux backend configured with `TOOLKIT=X11`

- `build/linux-sdl2/`
  - toolkit build tree for the Linux backend configured with `TOOLKIT=SDL2`

These build trees are kept separate so that toolkit-specific CMake cache data,
dependencies, and generated files do not overwrite each other.

## Docker-backed Linux targets

The Linux toolkit builds are driven by custom targets in the top-level
`CMakeLists.txt`.

Those targets run CMake inside Docker images that already contain the required
toolchain and system headers.

- `docker-x11`
- `docker-sdl2`

The images are:

- `wischner/gcc-x86_64-linux-x11`
- `wischner/gcc-x86_64-linux-sdl`

The source tree is mounted into the container at the same absolute path that it
has on the host. This keeps CMake build trees and cache paths stable between
host-side and Docker-side invocation.

## Root project structure

The root `CMakeLists.txt` adds two code subtrees:

- `src/`
- `examples/`

The current top-level project does not build generated API documentation.
The book in `docs/the-book/` is maintained as source documentation only.

## Core library target

The `native` library is defined in `src/CMakeLists.txt`.

The shared target contains portable code such as:

- geometry
- screen metadata
- application startup
- common graphics state

The same target is then extended by platform and toolkit subdirectories.

## Platform selection

Platform code lives under `src/platforms/`.

The platform subtree is selected with standard CMake platform variables:

- `WIN32`
- `HAIKU`
- `APPLE`
- `UNIX`

On Linux, the platform layer is combined with a toolkit layer.

## Toolkit selection

Toolkit code lives under `src/toolkits/`.

The active toolkit is selected through the cached `TOOLKIT` variable in
`src/toolkits/CMakeLists.txt`.

Only one toolkit subtree is added per configured build tree.

That is why Linux toolkit builds use different directories instead of sharing
one `build/` directory.

## Examples

Examples are built from `examples/` and link against `native`.

Today the repository includes:

- a minimal application window example
- a painter example

The Linux outputs are produced inside the toolkit-specific build trees:

- `build/linux-x11/examples/...`
- `build/linux-sdl2/examples/...`

## Summary

- CMake is the build entry point.
- `out/` is the host control tree.
- Linux toolkit builds run inside Docker.
- `build/linux-x11/` and `build/linux-sdl2/` are separate on purpose.
- The root project currently builds code and examples, not generated API docs.
