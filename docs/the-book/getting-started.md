# Getting Started

This chapter explains how to build the project as it works today and how to run
the example programs that come with the repository.

## Prerequisites

You need:

- CMake 3.16 or newer
- Docker

The Linux toolkit builds are performed inside Docker so that the required
compiler and system development packages come from known images rather than the
host machine.

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

This produces the host-side targets that launch the Linux toolkit builds.

## Build the Linux toolkit targets

Build the Linux toolkit target backed by the native window-system image:

```bash
cmake --build out --target docker-x11
```

Build the Linux toolkit target backed by the SDL-based image:

```bash
cmake --build out --target docker-sdl2
```

## Build outputs

The generated outputs are placed in separate toolkit build trees:

- `build/linux-x11/`
- `build/linux-sdl2/`

The `out/` directory is different: it is only the host-side control tree for
the top-level CMake project.

## Run the examples

Examples are produced inside the toolkit-specific build directories.

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
```

## What this chapter avoids

This chapter describes only the workflow that is currently maintained.
It does not document deferred API documentation generation or unfinished porting
work.
