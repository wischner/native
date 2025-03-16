![status.badge] [![language.badge]][language.url] [![standard.badge]][standard.url] [![license.badge]][license.url]

 # Welcome to native

*by Tomaz Stih*

**native** is a minimal, cross-platform UI library targeting **Linux**, **Windows**, and **Haiku**. It is designed to serve as a lightweight abstraction layer over the native UI toolkits provided by each operating system, exposing a consistent and modern C++ interface.

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
- **Cross-platform support**: Linux (X11), Windows (WinAPI), Haiku (API)
- **Native controls**: Direct use of system-native widgets and event loops  
- **Minimal and modern C++**: Clean code, few dependencies  
- **Educational**: Open development process, detailed documentation in chapters  
- **Consistent lowercase API**: Naming inspired by the C++ standard library


## Table of Contents

| Chapter | Description |
|---------|-------------|
| [**0. build system**](docs/chapter-0_build-system.md) | Project structure, build setup, and how to get started |
| [**1. application**](docs/chapter-1_application.md) | Core application class and startup/shutdown on Linux, Windows, and Haiku |


[language.url]:   https://isocpp.org/
[language.badge]: https://img.shields.io/badge/language-C++-blue.svg

[standard.url]:   https://en.wikipedia.org/wiki/C%2B%2B#Standardization
[standard.badge]: https://img.shields.io/badge/C%2B%2B-20-blue.svg

[license.url]:    https://github.com/tstih/nice/blob/master/LICENSE
[license.badge]:  https://img.shields.io/badge/license-MIT-blue.svg

[status.badge]:  https://img.shields.io/badge/status-unstable-red.svg