# Getting Started

This chapter explains the build and run workflow that is currently used and
verified.

## Verified runtime scope

- Verified runtime in this workflow:
  - Linux X11
  - Linux SDL2
  - Windows build through MinGW, run through Wine
  - Haiku cross-build, copied to a Haiku machine and run over SSH
- Build-verified but not runtime-verified in this workflow:
  - Linux OpenMotif
- Implemented but not runtime-verified in this workflow:
  - Apple
- Other toolkits/ports:
  - still work in progress

## Prerequisites

You need:

- CMake 3.20 or newer
- a compiler with C++20 support
- Docker

Backend builds are performed inside Docker so that the required compilers and
system development packages come from known images rather than the host
machine.

The X11 image provides Xlib, Xrandr, pixman, Xt, and Athena Widgets (Xaw).
The host-side control tree does not require those backend development
packages.

## Clone the repository

```bash
git clone https://github.com/tstih/native.git
cd native
```

## Configure the host control tree

Create the top-level CMake control tree:

```bash
cmake -S . -B build/cmake
```

This produces the host-side targets that launch backend builds.

## Build the backend targets

Build the Linux toolkit target backed by the native window-system image:

```bash
cmake --build build/cmake --target docker-x11
```

Build the GEMix toolkit target:

```bash
cmake --build build/cmake --target docker-gemix
```

Build the Linux toolkit target backed by the SDL-based image:

```bash
cmake --build build/cmake --target docker-sdl2
```

Build the Linux toolkit target backed by the OpenMotif image:

```bash
cmake --build build/cmake --target docker-openmotif
```

Build the Windows MinGW-w64 target:

```bash
cmake --build build/cmake --target docker-win
```

Build the Haiku target:

```bash
cmake --build build/cmake --target docker-haiku
```

Note:

- `docker-win` is part of the current verified workflow.
  It produces MinGW Windows binaries, and those binaries are run through Wine in this workflow.
- `docker-openmotif` is part of the current build-verified workflow.
  It produces OpenMotif-linked Linux binaries in a separate build tree.
- `docker-haiku` is part of the current verified workflow.
  It produces Haiku binaries locally, and those binaries are copied to a Haiku machine for runtime checks.
- Apple platform code exists, but there is no current Docker backend target for Apple builds in this repository.

## Build outputs

The generated outputs are placed in separate backend build trees:

- `build/linux-x11/`
- `build/linux-sdl2/`
- `build/linux-openmotif/`
- `build/windows-mingw-w64/`
- `build/haiku/`

The `build/cmake/` directory is different: it is only the host-side control tree for
the top-level CMake project.

## Run Vision

Every backend builds the same `vision` application from `src/`.

Run a native window-system, SDL2, or OpenMotif build directly:

```bash
./build/linux-x11/src/vision
./build/linux-sdl2/src/vision
./build/linux-openmotif/src/vision
```

The Windows cross-build produces
`build/windows-mingw-w64/src/vision.exe`. MinGW runtime DLLs must be beside
the executable when it is launched through Wine.

The Haiku cross-build produces `build/haiku/src/vision`. Copy that binary to
the Haiku machine before running it.

Status:

- Linux X11/SDL2, Windows/Wine, and Haiku SSH runs are currently exercised.
- Linux OpenMotif runs depend on host OpenMotif runtime availability.
- Apple runs are not yet exercised in this workflow.

For Haiku runtime checks in the current workflow, the binaries are copied to a
Haiku machine and launched there. The repository includes VS Code tasks and
launch entries for that deploy-and-run path.

## Repository layout

The main folders used in daily work are:

```text
include/              public C++ interface
lib/native/           native library implementation
src/                  Vision application
docs/manuals/         programming and implementation manuals
docs/notes/           exceptional notes only
build/cmake/          host CMake control tree
build/linux-x11/      Linux toolkit build tree
build/linux-sdl2/     Linux toolkit build tree
build/linux-openmotif/ Linux OpenMotif build tree
build/windows-mingw-w64/ Windows MinGW-w64 build tree
build/haiku/          Haiku build tree
```

## What this chapter avoids

This chapter describes only the workflow that is currently maintained.
It does not document deferred API documentation generation.
