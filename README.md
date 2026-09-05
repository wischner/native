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
  OPEN LOOK/XView, Window Maker/WINGs, and GEMix), Windows (WinAPI),
  Haiku (API), and macOS (Cocoa)
- **Native controls**: Direct use of system-native widgets and event loops
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
- SDL2 uses SDL2 and Xlib on Linux; SDL2_ttf is optional and enables system
  font loading. Xlib supplies image clipboard formats missing from SDL2.
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
- Windows MinGW-w64 into `build/windows-mingw-w64/`
- Haiku into `build/haiku/`

Each build contains the `native` static library and the single `vision`
application. Library implementation lives in `lib/native/`; application code
lives in `src/`.

Current exercised runtime paths are:

- Linux X11
- Linux SDL2
- Linux OPEN LOOK/XView in the `Tribblix-OpenLook` KVM guest
- Linux Window Maker/WINGs in the `Bookworm-WindowMaker` KVM guest
- Linux GEMix through Docker with the local rasta viewer, using either
  direct AES/VDI or the separate **Linux GEMix, gemd proxy** debug configuration
- Windows MinGW binaries run through Wine
- Haiku binaries built locally through Docker, then copied to a Haiku machine and run there

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
