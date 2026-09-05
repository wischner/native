# Getting Started

This chapter explains the build and run workflow that is currently used and
verified.

## Verified runtime scope

- Verified runtime in this workflow:
  - Linux X11
  - Linux SDL2
  - Linux OpenMotif under Xvfb in its Docker image
  - Linux OPEN LOOK/XView in the `Tribblix-OpenLook` KVM guest
  - Linux Window Maker/WINGs in the `Bookworm-WindowMaker` KVM guest
  - Windows build through MinGW, run natively in the Windows 11 VM
  - Haiku cross-build, copied to a Haiku machine and run over SSH
  - Apple on the configured remote macOS host
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
The OPEN LOOK image provides XView, OLGX, Xlib, Xrandr, and libtirpc.
The Window Maker image provides WINGs, WUtil, wraster, Xlib, Xrandr, and its
Pango/Xft font stack.
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
cmake --build build/cmake --target docker-gemix-gemd
```

The second target selects `GEMIX_USE_GEMD=ON` and writes to
`build/linux-gemix-gemd/`. Select **Debug Vision (Linux GEMix, gemd proxy)**
in VS Code to start a private server and Rasta viewer automatically. The
existing **Linux GEMix, local** configuration keeps using in-process AES/VDI.
The Docker image must include matching `libgem` and `/opt/gemix/bin/gemd`.

Build the Linux toolkit target backed by the SDL-based image:

```bash
cmake --build build/cmake --target docker-sdl2
```

Build the Linux toolkit target backed by the OpenMotif image:

```bash
cmake --build build/cmake --target docker-openmotif
```

Build the Linux OPEN LOOK target backed by XView and OLGX:

```bash
cmake --build build/cmake --target docker-openlook
```

Build the Window Maker target backed by WINGs:

```bash
cmake --build build/cmake --target docker-wmaker
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
  It produces MinGW Windows binaries for native execution in the Windows 11 VM.
- `docker-openmotif` is part of the current build-verified workflow.
  It produces OpenMotif-linked Linux binaries in a separate build tree.
- `docker-openlook` produces the reproducible XView-linked binary in
  `build/linux-openlook/`. The maintained graphical debug/runtime path builds
  the same backend natively in the `Tribblix-OpenLook` guest.
- `docker-wmaker` produces the reproducible WINGs-linked binary in
  `build/linux-wmaker/`. The maintained graphical debug/runtime path builds
  the same backend natively in the `Bookworm-WindowMaker` guest.
- `docker-haiku` is part of the current verified workflow.
  It produces Haiku binaries locally, and those binaries are copied to a Haiku machine for runtime checks.
- Apple platform code exists, but there is no current Docker backend target for Apple builds in this repository.

## Build outputs

The generated outputs are placed in separate backend build trees:

- `build/linux-x11/`
- `build/linux-sdl2/`
- `build/linux-openmotif/`
- `build/linux-openlook/`
- `build/linux-wmaker/`
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

OPEN LOOK and Window Maker need the window manager that belongs to them:
`olwm` owns OPEN LOOK's frames and resize behaviour, and Window Maker owns its
own. Started on an ordinary desktop they come up under whatever window manager
is already running, and neither behaves as its toolkit intends. The maintained
VS Code launch entries start their libvirt guests, synchronize the source,
build it natively, and debug Vision on the logged-in desktop over SSH.

The Docker build artifacts can still be exercised locally through the isolated
smoke-session script, which raises Xephyr and starts the matching window
manager inside it:

```bash
./scripts/linux/toolkit-session-run.sh openlook ./build/linux-openlook/src/vision
./scripts/linux/toolkit-session-run.sh wmaker ./build/linux-wmaker/src/vision
```

These nested sessions are useful for build-image checks but are separate from
the VM-native F5 workflow documented in [Build System](BUILD-SYSTEM.md).

The Windows cross-build produces
`build/windows-mingw-w64/src/vision.exe`. MinGW runtime DLLs must be beside
the executable in Windows. The **Debug Vision (Windows 11 VM)** F5 profile
deploys the program, matching DLLs and native debug server to the autologon
desktop. It does not use Wine. See the
[Windows VM note](../../notes/WINDOWS-VM-RUNTIME.md) for one-time SSH setup.

The Haiku cross-build produces `build/haiku/src/vision`. Copy that binary to
the Haiku machine before running it.

The X11 F5 profile, **Debug Vision (Linux X11, Xephyr/TWM mono)**, builds
through Docker and starts GDB inside a dedicated monochrome Xephyr/TWM
session. It chooses a free nested display and removes its session when
debugging ends; other backend profiles keep their existing environments.
See [Build System](BUILD-SYSTEM.md#monochrome-x11-runtime-session) for host tools
and the equivalent standalone launcher.

Status:

- Linux X11/SDL2, OPEN LOOK in Tribblix, Window Maker in Bookworm,
  Windows, and Haiku SSH runs are currently exercised.
- Linux OpenMotif lifecycle checks run in the Motif Docker image under
  Xvfb.
- Apple builds, startup smoke tests, and lifecycle assertions run on the
  configured remote macOS host.

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
build/linux-openlook/ Linux OPEN LOOK/XView build tree
build/linux-wmaker/  Linux Window Maker/WINGs build tree
build/windows-mingw-w64/ Windows MinGW-w64 build tree
build/haiku/          Haiku build tree
```

## What this chapter avoids

This chapter describes only the workflow that is currently maintained.
It does not document deferred API documentation generation.
