# The Book of Native

This book explains the Native architecture, the implementation patterns used
to preserve it, and the current build and backend status. It is the detailed
companion to the concise, normative
[Architectural Standards](../standards/ARCHITECTURE.md).

For writing applications, use [Programming Native](PROGRAMMING-NATIVE.md),
which has a control index and step-by-step programs. For AI-assisted application
work, use the [generation guide](../ai/README.md). This book is for understanding
or changing the library implementation.

The standards define what the code must do. These chapters explain why the
rules exist, how shared and backend code divide the work, and what an
implementation should look like. This is not a roadmap or a wish list. When
the architecture or code changes, the relevant chapter should be updated in
the same commit.

## Current scope (October 2026)

Settings navigation and civil date/time pickers are implemented on every
maintained backend. Granite, AppKit, Win32 and Haiku use dedicated native
widgets; Windows settings navigation hosts the actual WinUI 3 NavigationView
with native search, using Windows App SDK 1.8. The other toolkits compose
existing native controls where available.
The shared contract covers stable page IDs, optional images, silent setters,
holiday/weekday restrictions, predicates and minute steps. The input/chrome
chapter explains peer ownership; the feature matrix distinguishes current
runtime assertions from cross-build evidence. Vision's `--settings` window
provides the interactive acceptance view.

The mouse extension adds client-scoped hidden policy, ordered boundary and
cancellation events, rich motion and capability-checked scoped capture. The
portable `shader_view` uses a shared programmable CPU image executor with
multipass/history and a separate optional OpenGL GLSL profile. Capabilities
reflect the live renderer, including immediately after transactional live
replacement. Failed candidates preserve the previous renderer and output.
Current per-backend build and new
runtime evidence are recorded separately in the feature matrix.


- Generic GTK Wayland passes 20/20 headless tests after the picker, combo,
  disclosure and close-lifetime fixes. Its native runtime fixture also exits
  normally on the host Wayland desktop, with 120 theme pixel comparisons.
- GTK 4.14 Wayland and elementary OS adapters are separately buildable and
  exercised under headless Weston (20/20 tests each), including the actual
  elementary stylesheet. The elementary OS 8.1 Pantheon desktop also passes
  focused native control checks and the table/combo/splitter/border walkthrough. The generic VM recheck hit
  GTK/compositor initialization timeouts; all three affected fixtures passed
  focused reruns after recovery, as documented in the VM note. They use
  native GTK controls and Granite 7
  C interfaces, with the advanced-table and interactive desktop
  limits listed in the feature matrix. Manual desktop acceptance limits are
  recorded in
  the [VM note](../notes/ELEMENTARY-VM-RUNTIME.md).
- Runtime-tested in this project workflow:
  - Linux X11 backend: 21/21 tests under Xvfb and TWM, including native
    modal panels, editable fields, date calendars and scrollbar input
  - Linux SDL2 backend: 27/27 tests, including queued control input,
    editable combo/picker fields, completion acceptance, table modifiers,
    nested canvas pixels, pointer capture and close-policy regressions. The
    control fixture also passes through the normal desktop GDB path
  - Linux OpenMotif backend: 25/25 headless tests plus the CDE desktop
    control fixture, including native picker fields and centered panels
  - Linux OPEN LOOK/XView backend in Docker with Xephyr and olwm: 24/24
    tests, including native input, editable-combo/picker composition and
    background damage that preserves windowless text items
  - Linux Window Maker/WINGs backend in Docker with Xephyr and Window Maker;
    classic input/picker/panel checks pass on the desktop, while full CTest
    reports 20/22 due to Fontconfig/Pango exit allocations
  - Linux GEMix backend through Docker and local rasta, both direct AES/VDI
    and serialized libgem/gemd builds (20/21 and 21/22; existing SDK menu
    restoration failure). SDK extended selectors, standard alerts and packet-driven input are used;
    direct-backend regressions also
    cover nested tab/split/canvas framebuffer composition and secondary
    canvas button identity. On 2026-10-21, full Rasta desktop colors and
    viewer-driven GDB/MI shutdown pass with 32-bit output and inversion off;
    the 2026-10-22 full suites pass 20/21 direct and 21/22 proxy tests,
    with the remaining final menu-strip restoration assertion failure
  - Windows backend (Docker MinGW build, run natively in the Windows 11 VM)
  - Haiku backend (Docker cross-build, deploy and run over SSH)
  - macOS AppKit backend (AppleClang 17 on VM `mike`, 19/19 CTests in the
    2026-10-21 Debug run; interactive LLDB stops and session shutdown verified)

The suite counts above describe dated runs, not an automatically updated
qualification of every later revision. Runtime-tested does not imply an exhaustive interactive walkthrough. The
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


## Earlier verification records

Linux GLSL context coexistence is fixed in the EGL adapter: it preserves and
restores the surrounding EGL/GLX toolkit context, including accelerated SDL
renderers. Context failures now include the operation and EGL error code.
The original GLSL pipeline was verified on an Intel Iris Xe desktop. The
2026-10-14 macOS Debug audit on `leia` passes four focused tests, including
real CGL context preservation, and the original GLSL terminal smoke. It does
not establish visual acceptance or frame-rate guarantees. The earlier Windows
VM suite passed 16 tests with two desktop-OpenGL skips. That Haiku run passed 18
regression executables with Mesa softpipe, including real GLSL rendering and
context ownership, as well as the portable CPU profile. The feature matrix
records the exact scope of these target runs.


The 2026-10-15 review, completed non-Amiga adjustments, dated verification
and narrow SDK sanitizer exceptions are in [ADJUST.md](../ADJUST.md).
Passing regressions do not establish exhaustive standards conformance.
`docker-amiga-check` qualifies runtime prerequisites only and currently fails;
AmigaOS is not an implemented backend.
