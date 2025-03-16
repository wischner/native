# Chapter 0: build system and directory organization

This chapter outlines the **build system** and **directory structure** used for the `native` library.

The project uses **CMake** for its build system, with the goal of being simple, cross-platform, and easy to extend as more functionality is added.

## Build System Overview

`native` uses **CMake**, a widely adopted and portable build system generator.  
- It supports Linux, Windows, and Haiku out of the box.
- It allows for clear separation of platform-specific code.
- It provides flexibility to include multiple build targets (static libraries, shared libraries, tests, examples).

### Requirements
- CMake 3.16 or higher
- A C++20 compliant compiler  
  - GCC 10+, Clang 11+, or MSVC 2019+

---

## Directory Structure

The project is organized as follows:

## Directory Structure

```mermaid
graph TD
    A(native/)
    A --> B[CMakeLists.txt]
    A --> C(include/)
    C --> C1[native.hpp]
    A --> D(src/)
    D --> D1[app.cpp]
    A --> E(platform/)
    E --> E1[linux/]
    E1 --> E1a[app_linux.cpp]
    E --> E2[windows/]
    E2 --> E2a[app_windows.cpp]
    E --> E3[haiku/]
    E3 --> E3a[app_haiku.cpp]
    A --> F(examples/)
    F --> F1[hello_world/]
    F1 --> F1a[CMakeLists.txt]
    F1 --> F1b[main.cpp]
    A --> G(chapters/)
    G --> G1[chapter-0_build-system.md]
    G --> G2[chapter-1_application.md]
```

This directory layout separates platform-independent code, platform-specific implementations, examples, and documentation into clear and distinct locations.