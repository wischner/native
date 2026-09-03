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

## Current scope (September 2026)

- Runtime-tested in this project workflow:
  - Linux X11 backend
  - Linux SDL2 backend
  - Linux OpenMotif backend
  - Linux OPEN LOOK/XView backend in the `Tribblix-OpenLook` KVM guest
  - Linux Window Maker/WINGs backend in the `Bookworm-WindowMaker` KVM guest
  - Windows backend (MinGW build, run through Wine)
  - Haiku backend (Docker cross-build, deploy and run over SSH)
  - Apple backend
- Work in progress:
  - other toolkit targets and ports not listed above

## Chapters

1. [Getting started](book-of-native/GETTING-STARTED.md)
   Build the project, run Vision, and understand the output layout.

2. [Build system](book-of-native/BUILD-SYSTEM.md)
   How top-level CMake and Docker-backed backend targets are organized.

3. [Patterns: source layering and native bindings](book-of-native/PATTERNS-LAYERING.md)
   Public API boundaries, the three implementation layers, and native
   handle/object mappings. Expands Architecture Sections 1 and 2.

4. [Patterns: geometry and type conventions](book-of-native/PATTERNS-GEOMETRY.md)
   Shared value types used by geometry, windows, graphics, and events.

5. [Patterns: signal and event dispatching](book-of-native/PATTERNS-SIGNALS.md)
   Public event contracts, connection lifetimes, and synchronous dispatch.
   Expands Architecture Section 3.

6. [Patterns: cached properties](book-of-native/PATTERNS-PROPERTIES.md)
   Setter/getter naming, portable caches, and native-originated updates.
   Expands Architecture Section 4.

7. [Patterns: windows and app windows](book-of-native/PATTERNS-WINDOWS.md)
   Window state, hierarchy, lifecycle, layout, and backend obligations.
   Expands Architecture Section 5.

8. [Patterns: window painting](book-of-native/PATTERNS-PAINTING.md)
   Graphics contexts, paint-event lifetimes, clipping, and invalidation.
   Expands Architecture Section 6.

9. [Patterns: custom and themed drawing](book-of-native/PATTERNS-THEME.md)
   Native theme primitives, semantic control states, and portable fallbacks.
   Expands Architecture Section 7.

10. [Drawing primitives](book-of-native/DRAWING-PRIMITIVES.md)
    Application reference for window and image graphics, PNG/JPEG I/O, text
    measurement, and themed control drawing.

11. [Patterns: application entry and main loop](book-of-native/PATTERNS-APPLICATION.md)
    Portable startup through `program()`, `app::run()`, and backend launchers.
    Expands Architecture Section 8.

12. [Patterns: screens and virtual desktops](book-of-native/PATTERNS-SCREENS.md)
    Snapshot lifetime, work areas, primary selection, and backend detection.
    Expands Architecture Section 9.

13. [Patterns: clipboard and text editing](book-of-native/PATTERNS-CLIPBOARD-TEXT-EDITING.md)
    Typed clipboard transactions, editor modes, live complete-value
    validation, selection, and direct or keyboard clipboard commands.
    Expands Architecture Sections 11 and 12.

14. [Patterns: advanced table views](book-of-native/PATTERNS-TABLE-VIEW.md)
    Model ownership, stable row identity, virtualization, grouping, search,
    and backend adaptation. Expands Architecture Section 13.

15. [Patterns: source editing](book-of-native/PATTERNS-CODE-EDIT.md)
    Canonical source storage, overlays, lexers, completion, shared painting,
    and backend input translation. Expands Architecture Section 14.

16. [Patterns: split views and tabs](book-of-native/PATTERNS-SPLIT-VIEWS-AND-TABS.md)
    Split/tab layout ownership, native content transitions, floating shells,
    interaction, themed painting, and persistence. Expands Architecture
    Section 16.

17. [Patterns: input, standard dialogs, and window chrome](book-of-native/PATTERNS-INPUT-DIALOGS-WINDOW-CHROME.md)
    Combo and list boxes, directory and message dialogs, non-client rulers,
    status bars, native adaptation, and extensibility. Expands Architecture
    Section 17.

18. [Patterns: structural panels and paintable canvases](book-of-native/PATTERNS-PANELS-AND-CANVASES.md)
    Container and drawing-surface roles, explicit child lifecycle, chrome
    versus client geometry, 32-bit content bounds, scrollbar resolution,
    and painting order. Expands Architecture Sections 18 and 19.

19. [Feature matrix](book-of-native/FEATURE-MATRIX.md)
    Per-backend feature and test status for what is implemented now.

For a tutorial organized around complete programs, see
[Programming Native](PROGRAMMING-NATIVE.md).
