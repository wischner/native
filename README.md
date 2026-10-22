![status.badge] [![language.badge]][language.url] [![standard.badge]][standard.url] [![license.badge]][license.url]

# Welcome to native

_by Tomaz Stih_

**native** is a minimal, cross-platform UI library targeting **Linux**, **Windows**, **Haiku**, and **macOS**. It is designed to serve as a lightweight abstraction layer over the native UI toolkits provided by each operating system, exposing a consistent and modern C++ interface.

The focus is on **clarity**, **minimalism**, and **practicality**:

- Minimal code to achieve common UI tasks.
- Consistent, lowercase API design inspired by the C++ standard library.
- No console application support—this is strictly for graphical user interfaces.

## Why another UI library?

**native** is not intended to compete with larger frameworks. Instead, it serves two main purposes:

1. It provides a clean, modern C++ API for simple and native UI
   development across four operating systems.
2. It is written in the open, **chapter by chapter**, allowing developers to understand exactly how each component works.  
   The development process is transparent, aiming to demystify cross-platform UI programming.

If you are looking for a straightforward, understandable UI library, or if you want to learn how to build one from scratch, **native** may be of interest.

## Features

- **Backend coverage today**: Linux (X11/Athena, SDL2, OpenMotif,
  OPEN LOOK/XView, Window Maker/WINGs, GEMix, GTK Wayland, and elementary OS), Windows (Win32/WinUI),
  Haiku (API), and macOS (Cocoa)
- **Native controls**: Direct use of system-native widgets and event loops
- **Settings and civil pickers**: Menu-style `settings_sidebar` page builders
  with optional images and search, plus native date/time pickers with date ranges,
  holiday/weekday exclusions, availability predicates and minute steps
- **Classic trees**: Stable-ID hierarchical `tree_view` with native
  disclosure, selection, images, navigation, and scrolling on every backend
- **Structural panels**: Empty child containers that parent and lay out any
  control, nest freely, and work as tab or split content
- **Paintable canvases**: Child drawing surfaces with 32-bit content bounds,
  themed automatic scrollbars, and application-owned client painting
- **Docking workspaces**: Stable-ID split and tab layouts, draggable panes,
  modeless floating windows, pinning and edge auto-hide, compact captions,
  native-themed compass drop targets, and versioned persistence
- **Extensible controls**: Virtual behavior hooks and protected owner-draw
  stages whose base implementations retain standard native behavior
- **Standard file dialogs**: Native open/save panels with one portable modal
  result, `std::filesystem::path`, and filter model
- **Filesystem resources**: Exact-size native file/folder icons as PNG with
  generic fallbacks, plus typed special directories as `std::filesystem::path`
- **Clipboard and text editing**: Typed UTF-8/image clipboard transactions,
  native or emulated single-line/multiline editors, and live validation
- **Minimal and modern C++**: Clean code, few dependencies
- **Educational**: Open development process, detailed documentation in chapters
- **Consistent lowercase API**: Naming inspired by the C++ standard library

## Minimal working native app

```cpp
//
// Demonstrates the smallest complete application built with native.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>

// Start the application with a mutable main window.
int program(int, char **) {
    native::app_wnd window("Hello World!");
    return native::app::run(window);
}
```

## Building

The common build requirements are CMake 3.20 or newer and a GCC
toolchain with C++20 support. A direct Linux build also needs
`pkg-config` and the development packages for the chosen backend:

- X11 uses Xlib, Xrandr, pixman, Xt, and
  [Athena Widgets (Xaw)](https://xorg.freedesktop.org/releases/X11R7.7/doc/libXaw/libXaw.html).
- SDL2 uses SDL2 and Xlib on Linux. Optional SDL2_ttf loads stock fonts;
  optional Fontconfig resolves stock families directly without a shell.
  Missing either dependency falls back to bitmap stock text. Xlib supplies
  image clipboard formats missing from SDL2.
- Haiku date/time editors use the SDK's private shared-kit headers and
  `libshared`/`localestub`; the Docker SDK supplies them.
- Haiku GLSL optionally loads Mesa OSMesa at runtime. The tested Mesa 22.0.5
  requires GALLIUM_DRIVER=softpipe; its llvmpipe JIT is rejected before drawing.
  No Mesa development package is required by the Docker cross-build. CPU shader
  execution remains independent of this optional runtime.
- Linux graphics targets use libpng and libjpeg for PNG/JPEG image I/O;
  Windows, Haiku, and macOS use their native codec services.
- OpenMotif uses Xlib, Xt, and Motif.
- OPEN LOOK uses XView, OLGX, Xlib, Xrandr, and libtirpc. Its standard
  `File_chooser`, Panel controls, OpenMenu menus, Selection service, and OLGX
  painter remain native to the toolkit.
- Window Maker uses WINGs, WUtil, wraster, Xlib, and Xrandr. WINGs supplies
  its windows, menus, controls, text editors, standard file panels, clipboard
  selection service, stock fonts, and native-look drawing resources.
- File dialogs use the standard OS or toolkit panel on Windows, macOS, Haiku,
  OpenMotif, OPEN LOOK, Window Maker, and GEMix. X11/Athena prefers Zenity or
  KDialog and otherwise uses its Xaw browser. SDL2 consistently uses its
  library-owned themed C++ filesystem browser for file open, file save, and folder
  selection.
- Portable TrueType/OpenType fonts use the vendored `stb_truetype`
  rasterizer, so file- and memory-backed fonts have the same metrics
  and pixels on every backend.

Configure a direct debug build under the root `build/` directory. GCC
debug builds enable `-Wall -Wextra -pedantic` and the address and
undefined-behavior sanitizers by default.

```bash
cmake -S . -B build/linux-x11 -DCMAKE_BUILD_TYPE=Debug -DTOOLKIT=X11
cmake --build build/linux-x11
./build/linux-x11/src/vision
```

Linux, Windows, and Haiku builds can also be driven through Docker so
their backend headers and tools come from known images:

```bash
cmake -S . -B build/cmake
cmake --build build/cmake --target docker-gemix
cmake --build build/cmake --target docker-gemix-gemd
cmake --build build/cmake --target docker-x11
cmake --build build/cmake --target docker-sdl2
cmake --build build/cmake --target docker-openmotif
cmake --build build/cmake --target docker-openlook
cmake --build build/cmake --target docker-wmaker
cmake --build build/cmake --target docker-win
cmake --build build/cmake --target docker-haiku
```

Both GEMix build targets use the published `wischner/gcc-x86_64-gemix:latest`
image, which already contains the GEM `v1.0.0` SDK built from commit
`0df463d9ff4b4cbbdd0612ce700c9e11e5e33661`: headers and Rasta-backend
libraries in `/usr/local` with `gemix-*` pkg-config files, plus `gemd`,
`resgen`, the GEM resources, and the recorded revision in
`/opt/gemix/share/metadata/gem.version`. Docker pulls the image on first use;
no local SDK image build and no sibling GEM checkout is needed.

Run backend-independent tests from any hosted debug build:

```bash
ctest --test-dir build/linux-x11 --output-on-failure
```

The Docker-backed targets build:

- `X11` into `build/linux-x11/`
- `GEMix` into `build/linux-gemix/`
- `GEMix` with `libgem -> gemd -> AES/VDI` into `build/linux-gemix-gemd/`
- `SDL2` into `build/linux-sdl2/`
- `OpenMotif` into `build/linux-openmotif/`
- `OPENLOOK` into `build/linux-openlook/`
- `WMAKER` into `build/linux-wmaker/`
- Windows MinGW-w64/WinUI into `build/windows-mingw-w64/`
- Haiku into `build/haiku/`

`docker-win` first builds `docker-win-sdk`, the pinned
`native-windows-winui-sdk:24.04` image containing GCC 13 and generated
C++/WinRT headers for Windows App SDK 1.8. Windows settings navigation uses
the actual WinUI 3 NavigationView. Deploy its bootstrap DLL beside the
executable and install x64 Windows App Runtime 1.8 on the guest; the normal
VM preparation script stages the DLL and checks that prerequisite.

Each build contains the `native` static library and the single `vision`
application. Library implementation lives in `lib/native/`; application code
lives in `src/`.

Current exercised runtime paths are:

- Linux X11 in monochrome TWM inside Xephyr. Select **Linux X11, Xephyr** for F5, or run `bash scripts/linux/x11/run-twm.sh` after
  the Docker build. The session runs entirely from the X11 image and follows
  the desktop scale; details are in
  [Build System](docs/manuals/book-of-native/BUILD-SYSTEM.md#monochrome-x11-runtime-session).
- Linux SDL2, with nine CTests covering nested canvas composition, real
  SDL pointer capture, close-request policy, and the shared UI contracts.
- Linux OPEN LOOK/XView in Docker with Xephyr and olwm
- Linux Window Maker/WINGs in Docker with Xephyr and Window Maker
- **GEM, Rasta@SDL2** runs GEM through gemd with the image-provided SDL2
  Rasta viewer and GDB in `wischner/gcc-x86_64-gemix:latest`.
  Direct-backend regressions cover nested tab/split/canvas framebuffer
  composition and right/middle canvas button identity. The viewer uses 32-bit
  output with inversion off; Native drawing matches AES's white paper and
  black ink. The full-desktop screenshot and live GDB/MI viewer-close test
  pass. Closing Rasta also stops GDB, Vision and gemd. Complete suites pass
  19/20 direct and 20/21 proxy tests; the final menu-strip restoration
  assertion remains failing on the pinned SDK.
- Windows MinGW binaries run natively in the Windows 11 VM; select
  **Windows 11, VM** for F5 through `ssh whiskey`. Changed-file deployment
  and a local VM DLL cache reduce repeat startup work (host Python 3.11+). See the
  [VM setup and debugging note](docs/notes/WINDOWS-VM-RUNTIME.md).
- Haiku binaries built locally through Docker, then deployed and debugged
  through `ssh hotel`; select **Haiku, VM** for F5.
- macOS AppKit, built natively with AppleClang and run on VM `mike`.
  Stock controls retain AppKit painting, including native splitters, tabs,
  table grids and row backgrounds. Six CTests pass normally and with
  ASan/UBSan; targeted desktop checks cover image text, accordion scrolling
  and table contrast. The full-gallery interactive walkthrough is not yet
  complete. See [macOS remote runtime](docs/notes/MACOS-REMOTE-RUNTIME.md)
  for build, test and desktop-permission details.

The 2026-10-21 Debug build on macOS VM `mike` passes all 19 CTests.
LLDB Developer Tools authorization is enabled. The SSH debug session ends
automatically when Vision exits and retains interactive breakpoint stops.

The macOS remote build checks Apple Command Line Tools and CMake before
source synchronization, including `~/Applications/CMake.app` in its search
path. CMake probes `std::jthread`/`std::stop_token` and enables libc++
`-fexperimental-library` on Apple SDKs only when required.

Current additional build-verified path is:

- Linux OpenMotif through `docker-openmotif`

## The book of native

Explore the full, chapter-by-chapter explanation of how the **native** UI library is built.

[Read the book »](docs/manuals/BOOK-OF-NATIVE.md)

For application programming concepts and complete sample programs, read the
[Programming Native manual](docs/manuals/PROGRAMMING-NATIVE.md).

[language.url]: https://isocpp.org/
[language.badge]: https://img.shields.io/badge/language-C++-blue.svg
[standard.url]: https://en.wikipedia.org/wiki/C%2B%2B#Standardization
[standard.badge]: https://img.shields.io/badge/C%2B%2B-20-blue.svg
[license.url]: https://github.com/tstih/nice/blob/master/LICENSE
[license.badge]: https://img.shields.io/badge/license-MIT-blue.svg
[status.badge]: https://img.shields.io/badge/status-unstable-red.svg

The **Settings and date/time pickers** gallery (`vision --settings`, also in
Window) opens a separate window with searchable General, Dates and Time pages. Try weekend
and recurring holiday exclusions, exclude individual dates, change minute
steps and toggle seconds. The public API and a complete preferences-window
example are in [Chapter 17](docs/manuals/programming-native/17-INPUT-DIALOGS-AND-WINDOW-CHROME.md).

The **Properties and toolbars** gallery (`vision --properties`) demonstrates
compact text/number/boolean/choice properties, custom dropdown canvases with
value converters, individual outer-border edges,
and multiple native or themed toolbars on all four sides. Mac uses native flat
AppKit accessory buttons and preserves native rounded preview buttons. Toolbar icons support
16x16, 24x24, and 32x32 sizes with live toolbar resizing; tools may be ordinary buttons, independent
sticky toggles, or exclusive tool groups. See the
[input and chrome chapter](docs/manuals/programming-native/17-INPUT-DIALOGS-AND-WINDOW-CHROME.md)
for construction and event examples. Native X11/Motif/WINGs border masks use
Xext's Shape extension; the macOS adapter uses QuartzCore.

## Physical input and background services

Native provides physical key press/release/reset events, bounded signed
16-bit mono/stereo PCM output, lifetime-scoped UI delivery for C++20 workers,
and shell-free bounded child-process capture. The
[background-services chapter](docs/manuals/programming-native/19-BACKGROUND-WORK-AND-SERVICES.md)
explains ownership and cancellation. Run the selected Vision binary with
`--infinity-test` for keyboard, audio and parallel-work acceptance.

Non-SDL Linux audio loads optional `libasound.so.2` at runtime; absence of
ALSA/default output makes `audio_out::open()` fail explicitly. Windows uses
WASAPI, macOS AudioToolbox and Haiku the media kit. GEMix currently reports
physical held-key input unavailable because its pinned SDK/viewer path loses
raw release information. Current build/runtime evidence is recorded in the
feature matrix. A parent CMake project may link the `native` target through
`add_subdirectory()` without building Vision or Native's test suite.

## Pointer control and programmable image shaders

`wnd` now provides a hidden client cursor policy, enter/leave/cancel events,
rich absolute/relative motion and move-only capture leases. Capabilities expose
unavailable modes explicitly: SDL2 provides relative capture, X11-family and
Windows provide drag capture, and no adapter claims raw/unaccelerated motion.
See [Painting and input](docs/manuals/programming-native/02-PAINTING-AND-INPUT.md).

`shader_view` executes the same bounded programmable CPU image package on every
backend, including multipass/history and typed parameters. It uses the existing
canvas/image compositor; this CPU profile reports no GPU acceleration. The
separate `native-glsl-1` profile uses an owned OpenGL context, with live
capability reporting. Source-sized
passes suit small emulator frames; large effects can delay UI input. The
optional standard-library Python 3 packaging tool is a developer dependency
only. Run `vision --mouse-shaders` for the independently authored CRT demo and
native-editor/pointer acceptance. See the
[shader tutorial](docs/manuals/programming-native/20-IMAGE-SHADERS.md) and
[feature matrix](docs/manuals/book-of-native/FEATURE-MATRIX.md) for limits and evidence.

`vision --retro-terminal` executes the original cool-retro-term and Qt blur
GLSL pass bodies through a private offscreen OpenGL renderer.
It includes native command entry, amber/green/color presets and bounded shell
output. `--terminal-smoke` adds an automated paint-and-close acceptance check.
Keep the copied `retro-terminal/` assets and license notices beside Vision.
See [the terminal tutorial](docs/manuals/programming-native/20-IMAGE-SHADERS.md#terminal-acceptance-application)
for the selected effects, licensing, and the lack of PTY/full-screen program support.


Linux GLSL context coexistence is fixed in the EGL adapter: it preserves and
restores the surrounding EGL/GLX toolkit context, including accelerated SDL
renderers. Context failures now include the operation and EGL error code.
The original GLSL pipeline was verified on an Intel Iris Xe desktop. On
2026-10-14, the macOS Debug build on `leia` also passed four focused shader
and delivery tests (including real CGL context preservation) and the original
GLSL terminal smoke. These checks do not establish visual or performance
acceptance. Windows retains GLSL compile coverage; its VM lacks desktop OpenGL.
Haiku supports the portable CPU profile and optional software GLSL through Mesa
OSMesa, verified with GALLIUM_DRIVER=softpipe on the configured VM.


## Amiga runtime qualification and standards review

AmigaOS 3.1/m68k is not yet an implemented Native backend. The available
compiler's C++ runtime fails threading and filesystem qualification:

```bash
cmake -S . -B build/cmake
cmake --build build/cmake --target docker-amiga-check
```

This diagnostic uses the pinned image in `DOCKER_IMAGE_AMIGA`, records both
failures in `build/amiga-toolchain/`, and produces no Native library or Vision
binary. The matching VS Code task is **Check Amiga C++20 Runtime (Docker)**.
The [build chapter](docs/manuals/book-of-native/BUILD-SYSTEM.md) describes the
check; [ADJUST.md](docs/ADJUST.md) records required toolchain repairs, the
all-platform standards review, reproduced defects and adjustment acceptance.
Existing backend build selections remain unchanged. The 2026-10-15 repair
pass adds lifetime, rollback, geometry, real GLSL history, native-close and
worker-shutdown regressions. Linux passes 141/141 tests and macOS 17/17.
The native Windows VM passes 16 tests and skips two desktop-OpenGL tests.
Haiku passes all 18 regression executables with Mesa softpipe, including real
GLSL rendering and context ownership, without skips. Per-backend results and independently
reproduced SDK sanitizer exceptions are recorded in
[the feature matrix](docs/manuals/book-of-native/FEATURE-MATRIX.md).
Passing tests do not certify exhaustive standards compliance.

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

Two independent toolkits are available: `WAYLAND` uses GTK 4.14's GDK Wayland
backend; `ELEMENTARY` adds Granite 7 through its C API. Both are written in C++
and restrict GDK to Wayland, with no X11 fallback or X socket forwarding.
Custom theme primitives reuse actual GTK/Granite widget snapshots and GTK CSS
part rendering. Windows retain native vector drawing through the device
transform; memory images rasterize the same native nodes.

```bash
cmake -S . -B build/cmake
cmake --build build/cmake --target docker-wayland
cmake --build build/cmake --target docker-elementary
```

The targets build the shared local `native-wayland-sdk:24.04` image from
`scripts/linux/wayland/Dockerfile`. Dependencies include GCC, GTK 4.14.x,
Granite 7.4, GtkSourceView 5.12, GLib/GIO, `gsettings-desktop-schemas`
(required by GraniteTimePicker), Cairo/Pango/HarfBuzz, Fontconfig,
xkbcommon, PNG/JPEG and EGL/Mesa. The image also contains Weston, elementary
icons and the pinned elementary OS 8 stylesheet. Outputs are
`build/linux-wayland/src/vision` and `build/linux-elementary/src/vision`.

Select **Elementary OS, VM** to build with Docker, deploy to `ssh echo`,
and debug Vision in the VM's existing Wayland desktop. Its deployment task
bundles GtkSourceView privately; no system package installation is needed.

Select **Linux GTK4, Wayland** in F5 for Docker execution on a host
Wayland desktop. Elementary uses only **Elementary OS, VM**, with its
installed desktop theme. See
[Build System](docs/manuals/book-of-native/BUILD-SYSTEM.md#gtk-414-wayland-and-elementary-os)
for the headless Weston test command and image provenance. Headless widget,
collection and terminal checks do not replace a Pantheon desktop acceptance
pass; advanced table/custom-paint and pointer limits are listed in the
[feature matrix](docs/manuals/book-of-native/FEATURE-MATRIX.md).

The [elementary VM runtime note](docs/notes/ELEMENTARY-VM-RUNTIME.md) records
Docker binary deployment to `ssh echo`, native-theme pixel comparisons,
current VM test results, and remaining desktop acceptance limits.

Vision’s settings navigation uses embedded [Font Awesome Free icons](src/assets/navigation/README.md),
licensed separately under CC BY 4.0. No installed icon font is required.
