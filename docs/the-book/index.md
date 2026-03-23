# The Book of Native

This book documents how the current codebase works today.

It is not a roadmap and it is not a wish list.
Each chapter describes behavior and structure that exists in the repository.

When code changes, this book should be updated in the same commit.

## Current scope (March 2026)

- Runtime-tested in this project workflow:
  - Linux X11 backend
  - Linux SDL2 backend
  - Windows backend (MinGW build, run through Wine)
  - Haiku backend (Docker cross-build, deploy and run over SSH)
- Build-tested but not yet runtime-tested in this workflow:
  - Linux OpenMotif backend
  - Linux GNUstep backend
- Implemented but not yet runtime-tested in this workflow:
  - Apple backend
- Work in progress:
  - other toolkit targets and ports not listed above

## Chapters

1. [Getting started](getting-started.md)
   Build the project, run current examples, and understand the output layout.

2. [Build system](build-system.md)
   How top-level CMake and Docker-backed backend targets are organized.

3. [Patterns: source layering and native bindings](patterns-layering.md)
   Core layering model and native handle/object mappings.

4. [Patterns: geometry and type conventions](patterns-geometry.md)
   Shared value types used by geometry, windows, graphics, and events.

5. [Patterns: signal and event dispatching](patterns-signals.md)
   Event dispatch semantics using `signal<>`.

6. [Patterns: application entry and main loop](patterns-application.md)
   Startup sequence from `program()` to backend main loops.

7. [Patterns: windows and app windows](patterns-windows.md)
   Responsibilities of `wnd`, `app_wnd`, invalidation, paint flow, and caches.

8. [Feature matrix](feature-matrix.md)
   Per-backend feature and test status for what is implemented now.
