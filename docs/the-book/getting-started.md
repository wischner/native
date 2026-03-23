# Getting Started

This chapter explains the build and run workflow that is currently used and
verified.

## Verified runtime scope

- Verified runtime in this workflow:
  - Linux X11
  - Linux SDL2
  - Windows build through MinGW, run through Wine
- Implemented but not runtime-verified in this workflow:
  - Haiku
  - Apple
- Other toolkits/ports:
  - still work in progress

## Prerequisites

You need:

- CMake 3.16 or newer
- Docker

Backend builds are performed inside Docker so that the required compilers and
system development packages come from known images rather than the host
machine.

## Clone the repository

```bash
git clone https://github.com/tstih/native.git
cd native
```

## Configure the host control tree

Create the top-level CMake control tree:

```bash
cmake -S . -B out
```

This produces the host-side targets that launch backend builds.

## Build the backend targets

Build the Linux toolkit target backed by the native window-system image:

```bash
cmake --build out --target docker-x11
```

Build the Linux toolkit target backed by the SDL-based image:

```bash
cmake --build out --target docker-sdl2
```

Build the Windows MinGW-w64 target:

```bash
cmake --build out --target docker-win
```

Build the Haiku target:

```bash
cmake --build out --target docker-haiku
```

Note:

- `docker-win` is part of the current verified workflow.
  It produces MinGW Windows binaries, and those binaries are run through Wine in this workflow.
- `docker-haiku` is available as a cross-build target.
  It produces Haiku binaries, but runtime is not yet exercised in this workflow.
- Apple platform code exists, but there is no current Docker backend target for Apple builds in this repository.

## Build outputs

The generated outputs are placed in separate backend build trees:

- `build/linux-x11/`
- `build/linux-sdl2/`
- `build/windows-mingw-w64/`
- `build/haiku/`

The `out/` directory is different: it is only the host-side control tree for
the top-level CMake project.

## Run the examples

Examples are produced inside backend-specific build directories.

For the native window-system toolkit build:

```bash
./build/linux-x11/examples/01_app_example/app-example
./build/linux-x11/examples/02_painter_example/painter-example
```

For the SDL-based toolkit build:

```bash
./build/linux-sdl2/examples/01_app_example/app-example
./build/linux-sdl2/examples/02_painter_example/painter-example
```

For the Windows cross-build:

```bash
./build/windows-mingw-w64/examples/01_app_example/app-example.exe
./build/windows-mingw-w64/examples/02_painter_example/painter-example.exe
```

When running MinGW binaries through Wine, MinGW runtime DLLs must be available
next to each `.exe` (or provided through Wine path configuration).

For the Haiku cross-build:

```bash
./build/haiku/examples/01_app_example/app-example
./build/haiku/examples/02_painter_example/painter-example
```

Status:

- Linux X11/SDL2 and Windows/Wine runs are currently exercised.
- Haiku and Apple runs are not yet exercised in this workflow.

## Repository layout

The main folders used in daily work are:

```text
include/              public C++ interface
src/                  implementation
examples/             runnable samples
docs/the-book/        maintained internal documentation
docs/notes/           exceptional notes only
out/                  host CMake control tree
build/linux-x11/      Linux toolkit build tree
build/linux-sdl2/     Linux toolkit build tree
build/windows-mingw-w64/ Windows MinGW-w64 build tree
build/haiku/          Haiku build tree
```

## What this chapter avoids

This chapter describes only the workflow that is currently maintained.
It does not document deferred API documentation generation.
