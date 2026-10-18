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

## Current scope (October 2026)

The mouse extension adds client-scoped hidden policy, ordered boundary and
cancellation events, rich motion and capability-checked scoped capture. The
portable `shader_view` uses a shared programmable CPU image executor with
multipass/history and a separate optional OpenGL GLSL profile. Capabilities
reflect the live renderer, including immediately after transactional live
replacement. Failed candidates preserve the previous renderer and output.
Current per-backend build and new
runtime evidence are recorded separately in the feature matrix.


- Runtime-tested in this project workflow:
  - Linux X11 backend
  - Linux SDL2 backend, including nested canvas renderer pixels, real
    SDL pointer routing/capture, and close-policy regressions
  - Linux OpenMotif backend
  - Linux OPEN LOOK/XView backend in Docker with Xephyr and olwm
  - Linux Window Maker/WINGs backend in Docker with Xephyr and Window Maker
  - Linux GEMix backend through Docker and local rasta, both direct AES/VDI
    and serialized libgem/gemd builds; direct-backend regressions also
    cover nested tab/split/canvas framebuffer composition and secondary
    canvas button identity
  - Windows backend (Docker MinGW build, run natively in the Windows 11 VM)
  - Haiku backend (Docker cross-build, deploy and run over SSH)
  - macOS AppKit backend (native AppleClang build on remote `leia`, all 17
    current CTests passing, plus earlier targeted desktop checks; the older
    six-test suite also passed with ASan/UBSan)
- Work in progress:
  - other toolkit targets and ports not listed above

Runtime-tested does not imply an exhaustive interactive walkthrough. The
[feature matrix](book-of-native/FEATURE-MATRIX.md) records per-feature
coverage; the [macOS runtime note](../notes/MACOS-REMOTE-RUNTIME.md) separates
automated regressions and targeted visual checks from the still-incomplete
full-gallery walkthrough.

The portable full-monitor CRT shader demo is available as `vision --retro-terminal`.
Its selected upstream effects, shell and rendering tests, and current backend
limits are recorded in the [feature matrix](book-of-native/FEATURE-MATRIX.md).

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

13. [Patterns: stock and portable fonts](book-of-native/PATTERNS-FONTS.md)
    Byte ownership, private registrations, measurement and rasterization.
    Expands Architecture Section 10.

14. [Patterns: clipboard and text editing](book-of-native/PATTERNS-CLIPBOARD-TEXT-EDITING.md)
    Typed clipboard transactions, editor modes, live complete-value
    validation, selection, and direct or keyboard clipboard commands.
    Expands Architecture Sections 11 and 12.

15. [Patterns: advanced table views](book-of-native/PATTERNS-TABLE-VIEW.md)
    Model ownership, stable row identity, virtualization, grouping, search,
    and backend adaptation. Expands Architecture Section 13.

16. [Patterns: source editing](book-of-native/PATTERNS-CODE-EDIT.md)
    Canonical source storage, overlays, lexers, completion, shared painting,
    and backend input translation. Expands Architecture Section 14.

17. [Patterns: classic tree views](book-of-native/PATTERNS-TREE-VIEW.md)
    Owned hierarchy, stable IDs, visibility, disclosure and native adapters.
    Expands Architecture Section 15.

18. [Patterns: split views and tabs](book-of-native/PATTERNS-SPLIT-VIEWS-AND-TABS.md)
    Split/tab layout ownership, native content transitions, floating shells,
    interaction, themed painting, and persistence. Expands Architecture
    Section 16.

19. [Patterns: input, standard dialogs, and window chrome](book-of-native/PATTERNS-INPUT-DIALOGS-WINDOW-CHROME.md)
    File icons, special directories, combo and list boxes, directory and
    message dialogs, non-client rulers, status bars, native adaptation, and
    extensibility. Expands Architecture Section 17.

20. [Patterns: structural panels and paintable canvases](book-of-native/PATTERNS-PANELS-AND-CANVASES.md)
    Container and drawing-surface roles, explicit child lifecycle, chrome
    versus client geometry, 32-bit content bounds, scrollbar resolution,
    and painting order. Expands Architecture Sections 18 and 19.

21. [Feature matrix](book-of-native/FEATURE-MATRIX.md)
    Per-backend feature and test status for what is implemented now.

22. [Background delivery, PCM output, and child ownership](book-of-native/PATTERNS-BACKGROUND-SERVICES.md)
    Scoped C++20 UI results, bounded PCM peers and shell-free child ownership.
    Expands Architecture Section 20.

23. [Portable programmable image shaders](book-of-native/PATTERNS-IMAGE-SHADERS.md)
    Bounded CPU image programs, canvas composition, temporal history and
    coalesced generation-safe scheduling. Expands Architecture Section 21.

For a tutorial organized around complete programs, see
[Programming Native](PROGRAMMING-NATIVE.md).


Linux GLSL context coexistence is fixed in the EGL adapter: it preserves and
restores the surrounding EGL/GLX toolkit context, including accelerated SDL
renderers. Context failures now include the operation and EGL error code.
The original GLSL pipeline was verified on an Intel Iris Xe desktop. The
2026-10-14 macOS Debug audit on `leia` passes four focused tests, including
real CGL context preservation, and the original GLSL terminal smoke. It does
not establish visual acceptance or frame-rate guarantees. The final Windows
VM suite passes 16 tests with two desktop-OpenGL skips. Haiku passes all 18
regression executables with Mesa softpipe, including real GLSL rendering and
context ownership, as well as the portable CPU profile. The feature matrix
records the exact scope of these target runs.


The 2026-10-15 review, completed non-Amiga adjustments, current verification
and narrow SDK sanitizer exceptions are in [ADJUST.md](../ADJUST.md).
Passing regressions do not establish exhaustive standards conformance.
`docker-amiga-check` qualifies runtime prerequisites only and currently fails;
AmigaOS is not an implemented backend.
