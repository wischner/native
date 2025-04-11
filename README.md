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

- **Cross-platform support**: Linux (X11, SDL2, Motif, OpenLook), Windows (WinAPI), Haiku (API), macOS (Cocoa)
- **Native controls**: Direct use of system-native widgets and event loops
- **Minimal and modern C++**: Clean code, few dependencies
- **Educational**: Open development process, detailed documentation in chapters
- **Consistent lowercase API**: Naming inspired by the C++ standard library

## Minimal working native app

```cpp
#include "native.h"

int program(int argc, char* argv[])
{
    return native::app::run(app_wnd("Hello World!"));
}
```

## The book of native

Explore the full, chapter-by-chapter explanation of how the **native** UI library is built.

[Read the book »](docs/the-book/index.md)

[language.url]: https://isocpp.org/
[language.badge]: https://img.shields.io/badge/language-C++-blue.svg
[standard.url]: https://en.wikipedia.org/wiki/C%2B%2B#Standardization
[standard.badge]: https://img.shields.io/badge/C%2B%2B-20-blue.svg
[license.url]: https://github.com/tstih/nice/blob/master/LICENSE
[license.badge]: https://img.shields.io/badge/license-MIT-blue.svg
[status.badge]: https://img.shields.io/badge/status-unstable-red.svg
