# The Book of Native

This book explains how the current codebase works.

It is not a roadmap and it is not a wish list.
Each chapter should describe behavior, structure, and practices that already
exist in the repository.

If a feature is added later, the book can grow with it.

## Chapters

1. [Getting started](getting-started.md)
   Build the project, run the examples, and understand the build directories.

2. [Build system](build-system.md)
   How the top-level CMake project, Docker-backed Linux targets, and toolkit
   build trees are organized.

3. [Patterns: source layering and native bindings](patterns-layering.md)
   How the code separates public C++ abstractions from native implementation
   details and maps handles to objects.

4. [Patterns: geometry and type conventions](patterns-geometry.md)
   The basic value types used across windows, graphics, and events.

5. [Patterns: signal and event dispatching](patterns-signals.md)
   How event delivery works through the internal `signal<>` mechanism.

6. [Patterns: application entry and main loop](patterns-application.md)
   How `program()`, `app::run()`, screen detection, window creation, and the
   backend main loop fit together.

7. [Patterns: windows and app windows](patterns-windows.md)
   The role of `wnd`, `app_wnd`, invalidation, painting, and backend-specific
   native resources.

8. [Feature matrix](feature-matrix.md)
   The feature surface that is implemented in the currently stable backends.
