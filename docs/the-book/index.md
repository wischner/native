# The Book of Native

This book documents the internals of the **native** UI library — from the build system and core architecture to drawing, font rendering, and platform integration.

If you haven’t already, read the [README](../../README.md) first for an overview of what `native` is and what problems it solves.

## Chapters

1. [Getting started](getting-started.md)  
   How to build `native`, its structure, toolkits, and how to run examples.

2. [Build system](build-system.md)  
   Toolkit detection, per-platform source inclusion, and CMake practices.

3. [Patterns: source layering and native bindings](patterns-layering.md)  
   Describes the architecture for isolating native code, layering `src/`, `platforms/`, and `toolkits/`, and using the `bindings` mechanism.

4. [Patterns: geometry and type conventions](patterns-geometry.md)  
   Design of basic geometry types (`point`, `size`, `rect`) and the coordinate model used across toolkits.

5. [Patterns: signal and event dispatching](patterns-signals.md)  
   The design of `signal<>` and its role in window events, including connection, disconnection, and event propagation.

6. [Patterns: application entry and main loop](patterns-application.md)  
   Describes how applications start via the `program()` function, how the `app` class manages the main window and loop, and how platform-specific `main()` functions route into a common entry point.

7. [Patterns: windows and app windows](patterns-windows.md)  
   Base class for all windows (`wnd`), main application window (`app_wnd`), and how platforms implement native creation and event routing.
