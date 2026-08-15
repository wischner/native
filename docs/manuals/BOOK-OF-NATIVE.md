# The Book of Native

This book explains the Native architecture, the implementation patterns used
to preserve it, and the current build and backend status. It is the detailed
companion to the concise, normative
[Architectural Standards](../standards/ARCHITECTURE.md).

The standards define what the code must do. These chapters explain why the
rules exist, how shared and backend code divide the work, and what an
implementation should look like. This is not a roadmap or a wish list. When
the architecture or code changes, the relevant chapter should be updated in
the same commit.

## Current scope (August 2026)

- Runtime-tested in this project workflow:
  - Linux X11 backend
  - Linux SDL2 backend
  - Windows backend (MinGW build, run through Wine)
  - Haiku backend (Docker cross-build, deploy and run over SSH)
- Build-tested but not yet runtime-tested in this workflow:
  - Linux OpenMotif backend
- Implemented but not yet runtime-tested in this workflow:
  - Apple backend
- Work in progress:
  - other toolkit targets and ports not listed above

## Chapters

1. [Getting started](book-of-native/getting-started.md)
   Build the project, run Vision, and understand the output layout.

2. [Build system](book-of-native/build-system.md)
   How top-level CMake and Docker-backed backend targets are organized.

3. [Patterns: source layering and native bindings](book-of-native/patterns-layering.md)
   Public API boundaries, the three implementation layers, and native
   handle/object mappings. Expands Architecture Sections 1 and 2.

4. [Patterns: geometry and type conventions](book-of-native/patterns-geometry.md)
   Shared value types used by geometry, windows, graphics, and events.

5. [Patterns: signal and event dispatching](book-of-native/patterns-signals.md)
   Public event contracts, connection lifetimes, and synchronous dispatch.
   Expands Architecture Section 3.

6. [Patterns: cached properties](book-of-native/patterns-properties.md)
   Setter/getter naming, portable caches, and native-originated updates.
   Expands Architecture Section 4.

7. [Patterns: windows and app windows](book-of-native/patterns-windows.md)
   Window state, hierarchy, lifecycle, layout, and backend obligations.
   Expands Architecture Section 5.

8. [Patterns: window painting](book-of-native/patterns-painting.md)
   Graphics contexts, paint-event lifetimes, clipping, and invalidation.
   Expands Architecture Section 6.

9. [Patterns: custom and themed drawing](book-of-native/patterns-theme.md)
   Native theme primitives, semantic control states, and portable fallbacks.
   Expands Architecture Section 7.

10. [Drawing primitives](book-of-native/drawing-primitives.md)
    Application reference for window and image graphics, PNG/JPEG I/O, text
    measurement, and themed control drawing.

11. [Patterns: application entry and main loop](book-of-native/patterns-application.md)
    Portable startup through `program()`, `app::run()`, and backend launchers.
    Expands Architecture Section 8.

12. [Patterns: screens and virtual desktops](book-of-native/patterns-screens.md)
    Snapshot lifetime, work areas, primary selection, and backend detection.
    Expands Architecture Section 9.

13. [Feature matrix](book-of-native/feature-matrix.md)
    Per-backend feature and test status for what is implemented now.

For a tutorial organized around complete programs, see
[Programming Native](PROGRAMMING-NATIVE.md).
