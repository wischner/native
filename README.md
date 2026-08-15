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

1. It provides a clean, modern C++ API for simple and native UI development across three operating systems.
2. It is written in the open, **chapter by chapter**, allowing developers to understand exactly how each component works.  
   The development process is transparent, aiming to demystify cross-platform UI programming.

If you are looking for a straightforward, understandable UI library, or if you want to learn how to build one from scratch, **native** may be of interest.

## Features

- **Backend coverage today**: Linux (X11, SDL2, OpenMotif build path), Windows (WinAPI), Haiku (API), macOS (Cocoa code path)
- **Native controls**: Direct use of system-native widgets and event loops
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

// Start the application with a temporary main window.
int program(int, char **) {
    return native::app::run(native::app_wnd("Hello World!"));
}
```

## Building

The common build requirements are CMake 3.20 or newer and a GCC
toolchain with C++20 support. A direct Linux build also needs
`pkg-config` and the development packages for the chosen backend:

- X11 uses Xlib, Xrandr, and pixman.
- SDL2 uses SDL2; SDL2_ttf is optional and enables system font loading.
- OpenMotif uses Xlib, Xt, and Motif.

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
cmake --build build/cmake --target docker-x11
cmake --build build/cmake --target docker-sdl2
cmake --build build/cmake --target docker-openmotif
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
- `SDL2` into `build/linux-sdl2/`
- `OpenMotif` into `build/linux-openmotif/`
- Windows MinGW-w64 into `build/windows-mingw-w64/`
- Haiku into `build/haiku/`

Each build contains the `native` static library and the single `vision`
application. Library implementation lives in `lib/native/`; application code
lives in `src/`.

Current exercised runtime paths are:

- Linux X11
- Linux SDL2
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
