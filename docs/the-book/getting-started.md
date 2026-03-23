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
  - Linux GNUstep
- Implemented but not runtime-verified in this workflow:
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

Build the Linux toolkit target backed by the OpenMotif image:

```bash
cmake --build out --target docker-openmotif
```

Build the Linux toolkit target backed by the GNUstep image:

```bash
cmake --build out --target docker-gnustep
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
- `docker-openmotif` is part of the current build-verified workflow.
  It produces OpenMotif-linked Linux binaries in a separate build tree.
- `docker-gnustep` is part of the current build-verified workflow.
  It produces GNUstep-linked Linux binaries in a separate build tree.
- `docker-haiku` is part of the current verified workflow.
  It produces Haiku binaries locally, and those binaries are copied to a Haiku machine for runtime checks.
- Apple platform code exists, but there is no current Docker backend target for Apple builds in this repository.

## Build outputs

The generated outputs are placed in separate backend build trees:

- `build/linux-x11/`
- `build/linux-sdl2/`
- `build/linux-openmotif/`
- `build/linux-gnustep/`
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

For the OpenMotif toolkit build:

```bash
./build/linux-openmotif/examples/01_app_example/app-example
./build/linux-openmotif/examples/02_painter_example/painter-example
```

In the current workflow, these binaries are build-verified but not yet runtime-exercised.

For the GNUstep toolkit build:

```bash
XAUTH=${XAUTHORITY:-$HOME/.Xauthority}
docker run --rm \
  -e DISPLAY=$DISPLAY \
  -e XAUTHORITY=/tmp/.Xauthority \
  -v "$XAUTH":/tmp/.Xauthority:ro \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  -v "$PWD":/src \
  -w /src/build/linux-gnustep/examples/01_app_example \
  wischner/gcc-x86_64-linux-gnustep ./app-example

docker run --rm \
  -e DISPLAY=$DISPLAY \
  -e XAUTHORITY=/tmp/.Xauthority \
  -v "$XAUTH":/tmp/.Xauthority:ro \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  -v "$PWD":/src \
  -w /src/build/linux-gnustep/examples/02_painter_example \
  wischner/gcc-x86_64-linux-gnustep ./painter-example
```

Host-direct execution of GNUstep binaries is only expected to work when GNUstep
runtime libraries are installed on the host.

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

- Linux X11/SDL2, Windows/Wine, and Haiku SSH runs are currently exercised.
- Linux GNUstep runs are currently exercised through Docker runtime launch.
- Linux OpenMotif runs depend on host OpenMotif runtime availability.
- Apple runs are not yet exercised in this workflow.

For Haiku runtime checks in the current workflow, the binaries are copied to a
Haiku machine and launched there. The repository includes VS Code tasks and
launch entries for that deploy-and-run path.

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
build/linux-openmotif/ Linux OpenMotif build tree
build/linux-gnustep/  Linux GNUstep build tree
build/windows-mingw-w64/ Windows MinGW-w64 build tree
build/haiku/          Haiku build tree
```

## What this chapter avoids

This chapter describes only the workflow that is currently maintained.
It does not document deferred API documentation generation.
