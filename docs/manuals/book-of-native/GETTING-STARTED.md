# Getting Started

This chapter explains the build and run workflow that is currently used and
verified.

## Choose a backend

The maintained selections are GTK Wayland, elementary/Granite, X11/Athena,
SDL2, Motif/CDE, OPEN LOOK, Window Maker, GEMix, Windows, Haiku, and macOS.
GEMix also has a separate serialized gemd build. The
[feature matrix](FEATURE-MATRIX.md) distinguishes compilation, automated runtime
checks, desktop interaction, and unresolved issues. A successful build does
not imply an exhaustive visual walkthrough.

For writing application code rather than building the library, start with the
[Programming Native control index](../PROGRAMMING-NATIVE.md#find-every-control).
AI runners can use the [generation guide](../../ai/README.md).

## Prerequisites

You need:

- CMake 3.20 or newer
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

GTK Wayland and elementary use separate build trees and share the SDK-image
dependency. They do not start X11:

```bash
cmake --build build/cmake --target docker-wayland
cmake --build build/cmake --target docker-elementary
```


Build the Linux toolkit target backed by the native window-system image:

```bash
cmake --build build/cmake --target docker-x11
```

Both GEMix build targets use the published `wischner/gcc-x86_64-gemix:latest`
image, which already contains the GEM `v1.0.0` SDK built from commit
`0df463d9ff4b4cbbdd0612ce700c9e11e5e33661`: headers and Rasta-backend
libraries in `/usr/local` with `gemix-*` pkg-config files, plus `gemd`,
`resgen`, the GEM resources, and the recorded revision in
`/opt/gemix/share/metadata/gem.version`. Docker pulls the image on first use;
no local SDK image build and no sibling GEM checkout is needed.

Build the GEMix toolkit target:

```bash
cmake --build build/cmake --target docker-gemix
cmake --build build/cmake --target docker-gemix-gemd
```

The second target selects `GEMIX_USE_GEMD=ON` and writes to
`build/linux-gemix-gemd/`. Select **GEM, Rasta@SDL2**
for F5: the published image supplies GDB, gemd, GEM resources and the SDL2
Rasta viewer. No host Rasta checkout is needed. The local launch profile
has been removed; the direct AES/VDI build target remains available. Rasta
uses a non-inverted 32-bit framebuffer for white paper and black ink across
both controls and AES chrome. Closing Rasta ends the debugger, Vision and
gemd together.

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

Build the Windows MinGW-w64/WinUI target. Its `docker-win-sdk` dependency
builds `native-windows-winui-sdk:24.04`, including the pinned Windows App SDK
1.8 projections and GCC 13 cross-compiler:

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
  `build/linux-openlook/`. The maintained graphical debug/runtime path runs
  that binary inside the same image using Xephyr and olwm.
- `docker-wmaker` produces the reproducible WINGs-linked binary in
  `build/linux-wmaker/`. The maintained graphical debug/runtime path runs
  that binary in the same image using Xephyr and Window Maker.
- `docker-haiku` is part of the current verified workflow.
  It produces Haiku binaries locally, and those binaries are copied to a Haiku machine for runtime checks.
- Apple platform code exists, but there is no current Docker backend target for Apple builds in this repository.

## Build outputs

The generated outputs are placed in separate backend build trees:

- `build/linux-wayland/`
- `build/linux-elementary/`
- `build/linux-gemix/`
- `build/linux-gemix-gemd/`
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

Run using its matching F5 profile or Docker runtime. A direct launch requires
the same runtime libraries as the SDK and a compatible desktop:

```bash
./build/linux-x11/src/vision
./build/linux-sdl2/src/vision
```

OPEN LOOK and Window Maker need the window manager that belongs to them:
`olwm` owns OPEN LOOK's frames and resize behaviour, and Window Maker owns its
own. Started on an ordinary desktop they come up under whatever window manager
is already running, and neither behaves as its toolkit intends.
The OPEN LOOK F5 profile builds with `docker-openlook`, then starts GDB
inside the image's `openlook-xephyr` session. The **Linux Window Maker,
Xephyr** profile builds with `docker-wmaker` and uses its image's
`window-maker-xephyr` session.

Run the Docker-built programs with their image-provided window managers:

```bash
./scripts/linux/toolkit-session-run.sh openlook ./build/linux-openlook/src/vision
./scripts/linux/toolkit-session-run.sh wmaker ./build/linux-wmaker/src/vision
```

Both sessions use the host display as Xephyr's parent, forward Xauthority,
and run their window manager inside the nested display. Without a reachable
host display, each image supplies a headless Xvfb parent.

The Windows cross-build produces
`build/windows-mingw-w64/src/vision.exe`. MinGW runtime DLLs must be beside
the executable in Windows, together with
`Microsoft.WindowsAppRuntime.Bootstrap.dll`. Install x64 Windows App Runtime
1.8 in the guest for the genuine WinUI NavigationView. The **Windows 11, VM** F5 profile
deploys the program, matching DLLs and native debug server to the autologon
desktop through `ssh whiskey`. Preparation transfers only changed files and
keeps full symbols locally while staging a smaller executable. Loaded VM DLLs
are cached under `build/windows-vm/` for subsequent debugger launches. The host
requires Python 3.11+. Runtime execution and debugging use the native VM;
Wine in the SDK image runs only the build-time projection generator. See the
[Windows VM note](../../notes/WINDOWS-VM-RUNTIME.md) for one-time SSH setup.

The Haiku cross-build produces `build/haiku/src/vision`. The **Haiku, VM**
F5 profile prepares, deploys and debugs it through `ssh hotel`; the managed
libvirt domain remains `Haiku`.

The X11 F5 profile, **Linux X11, Xephyr**, builds
through Docker and starts GDB inside a dedicated monochrome Xephyr/TWM
session. It chooses a free nested display and removes its session when
debugging ends; other backend profiles keep their existing environments.
See [Build System](BUILD-SYSTEM.md#monochrome-x11-runtime-session) for the
session's image-provided tools, desktop scaling and the equivalent standalone
launcher.

Status:

- Linux X11/SDL2, OPEN LOOK in Docker, Window Maker in Docker,
  Windows, and Haiku SSH runs are currently exercised.
- Linux OpenMotif lifecycle checks run in the Motif Docker image under
  Xvfb.
- Apple builds, startup smoke tests, and lifecycle assertions run on the
  macOS VM at `ssh mike` using **macOS, VM**.

For Haiku runtime checks in the current workflow, the binaries are copied to a
Haiku machine and launched there. The repository includes VS Code tasks and
launch entries for that deploy-and-run path.

The macOS remote build checks Apple Command Line Tools and CMake before
source synchronization, including `~/Applications/CMake.app` in its search
path. CMake probes `std::jthread`/`std::stop_token` and enables libc++
`-fexperimental-library` on Apple SDKs only when required.

## Repository layout

The main folders used in daily work are:

```text
include/              public C++ interface
lib/native/           native library implementation
src/                  Vision application
docs/manuals/         programming and implementation manuals
docs/notes/           exceptional notes only
docs/skills/          reusable application-generation skills
docs/agents/          runner-neutral agent role instructions
docs/ai/              AI context routing and request/handoff template
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

## Input, sound and background-work acceptance

Run the selected backend's Vision binary with `--infinity-test`; this opens
portable diagnostics for held physical keys, focus/reset, stereo PCM sound,
coalesced worker progress/cancellation and a real helper child. The
[application services chapter](../programming-native/19-BACKGROUND-WORK-AND-SERVICES.md)
lists the manual checks. The normal Docker build targets also build the
service regressions; run CTest in that build image, or on the target OS for
cross-builds. GEMix's UI explicitly reports physical key input unavailable.

Run the selected Vision binary with `--mouse-shaders` for programmable CPU image
passes beside a native editor, cursor hiding and explicit pointer capture.
The demo package is embedded; no shader compiler or GPU service is required.
Use the [image-shader tutorial](../programming-native/20-IMAGE-SHADERS.md) to
build your own portable package with the optional Python 3 developer tool.

`vision --retro-terminal` runs the portable ANSI/CRT command-terminal test mode;
add `--terminal-smoke` to execute one host-shell command, paint and exit. The
build copies `retro-terminal/` assets beside Vision. Keep that directory and
its notices when deploying the binary. Use an optimized Docker build for CPU
shader performance; terminal commands are line-oriented without a PTY.


On Haiku, the original GLSL terminal additionally uses the optional Mesa OSMesa
runtime. With the tested Mesa 22.0.5 package, run
`GALLIUM_DRIVER=softpipe ./vision --retro-terminal`. The Haiku F5 profile sets
the same process environment. See the image-shader chapter for the software
renderer and foreign-context requirements.

## Amiga prerequisite check

AmigaOS is not a supported Native backend yet. To reproduce the current
compiler/runtime qualification through the host control tree, run
`cmake --build build/cmake --target docker-amiga-check`. It uses the pinned
Amiga image and writes `threads.log` and `filesystem.log` below
`build/amiga-toolchain/`. Both checks currently fail; no GUI executable is
created. This is also available through the VS Code **Check Amiga C++20 Runtime
(Docker)** task. See [the build chapter](BUILD-SYSTEM.md) for details and
[ADJUST.md](../../ADJUST.md) for the measured blockers.

### Motif/CDE in Xephyr

Select **Linux Motif/CDE, Xephyr** for F5. The configuration builds with
`docker-openmotif` and runs GDB through the image's `cde-xephyr` helper in
`wischner/gcc-x86_64-linux-motif:latest`, which supplies CDE, Xephyr,
headers and libraries. No CDE VM or SSH setup is required.
For standalone execution:

```bash
./scripts/linux/toolkit-session-run.sh openmotif ./build/linux-openmotif/src/vision
```

The helper starts CDE services as container root on Docker's isolated
network, then runs the command as its CDE user. It forwards the host
X display or supplies a headless Xvfb parent. Debugging retains ASan/UBSan
with LeakSanitizer disabled under GDB.

## GTK 4.14 Wayland and elementary OS

```bash
cmake -S . -B build/cmake
cmake --build build/cmake --target docker-wayland
cmake --build build/cmake --target docker-elementary
```

These targets build their shared `native-wayland-sdk:24.04` image first.
The separate outputs are `build/linux-wayland/src/vision` and
`build/linux-elementary/src/vision`. Select **Linux GTK4, Wayland** for
Docker execution on the host Wayland compositor, or **Elementary OS, VM**
for deployment to `ssh echo`. Both use Wayland; X11 is not used.

Both adapters are C++ using GTK 4.14 C APIs. Elementary adds Granite 7 C APIs
and the image's elementary stylesheet. See [Build System](BUILD-SYSTEM.md#gtk-414-wayland-and-elementary-os)
for dependencies, image provenance and headless Weston regressions. Window
placement is compositor-owned, and advertised monitor geometry is used as
work area; the feature matrix records the current desktop-acceptance limits.

### elementary OS VM debugging

Select **Elementary OS, VM** in VS Code and press F5. The pre-launch task
builds `docker-elementary`, then `scripts/linux/elementary-vm/deploy.sh`
copies Vision, all retro-terminal assets and a private GtkSourceView runtime
to `echo:/home/tomaz/Projects/native/run`. The debug pipe runs the VM's GDB
through SSH with its existing Wayland and D-Bus session, Cairo rendering,
and the installed desktop theme. It keeps stdout reserved for GDB/MI.
`ELEMENTARY_VM_HOST` overrides the default SSH alias `echo`; the remote
user/path remains `tomaz`. The VM must have GDB, rsync and an active Wayland
login session at `wayland-0`. See the elementary VM runtime note for observed
desktop test limits. For deployment without debugging, build the target and
run `scripts/linux/elementary-vm/deploy.sh` directly.


## Settings and date/time gallery

Run the selected Vision binary with `--settings`, or choose **Settings and
date/time pickers** in its Window menu. The separate window exercises native
page navigation, optional icons, holiday/weekend restrictions and time steps.
The GTK/elementary SDK includes `gsettings-desktop-schemas`, required by
GraniteTimePicker; keep that package installed on a native deployment too.
Haiku's Docker SDK supplies the shared-kit headers and static libraries for
its native date/time editors. No new F5 profile is needed.
