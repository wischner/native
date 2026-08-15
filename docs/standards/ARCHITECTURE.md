# Architectural Standards

This document defines the current architectural requirements for the native
library. It is intentionally incomplete and will be extended as the
architecture evolves.

Detailed explanations and examples are available in
[The Book of Native](../manuals/BOOK-OF-NATIVE.md).

## 1. Code Structure

Native is a modern C++20 library. Applications using the library must never be
exposed to platform-specific implementation code. They use a consistent C++
API whose behavior does not depend on the selected backend.

The basic code-structure rules are:

- Keep the pure C++ public frontend separate from platform-specific backends.
- Including `<native.h>` must be sufficient to make the complete public API
  available.
- Place all library implementation code under `lib/native/`.
- Keep class declarations in headers and class implementations in source files.
- When necessary, implement a class across three levels:
  - Place behavior shared by all platforms in `lib/native/`.
  - Place operating-system-specific behavior in
    `lib/native/platforms/<platform>/`.
  - Place behavior specific to a toolkit on a particular platform in
    `lib/native/toolkits/<toolkit>/`.

## 2. Native Bindings

Public C++ classes must not contain native types, even in private members. To
associate a public C++ object with one or more native resources, use an
internal binding that is never exposed outside the library.

The `native::bindings<A, B>` class provides a bidirectional mapping between
native objects and library objects.

Typical mappings include:

- A native window handle and its `native::wnd *` object.
- A `native::wnd *` object and its backend graphics cache.

This lets the backend:

- Find the `wnd` that owns a native event.
- Find the native handle associated with a given `wnd`.
- Keep renderer and graphics state outside the public window class.

Declare these bindings in a private `globals.h` file and define them in the
corresponding `globals.cpp` file. Place them in the platform namespace when a
platform does not use a toolkit. When it does, place them in a nested
`platform::toolkit` namespace.

Example:

~~~cpp
namespace windows
{
    native::bindings<HWND, native::wnd *> wnd_bindings;
    native::bindings<HFONT, native::font *> font_bindings;
}

namespace linux::x11
{
    native::bindings<Window, native::wnd *> wnd_bindings;
}
~~~

Place any backend-wide native helper functions or shared native structures in
these same files and namespace.

## 3. Signals

Use `native::signal` for public event notifications. Signal arguments must use
public Native types, never platform or toolkit types. Use `signal<>` for events
without arguments and a public event structure for related values.

~~~cpp
signal<point> on_move;
int connection = on_move.connect(this, &window::handle_move);
on_move.emit(point(x, y));
on_move.disconnect(connection);
~~~

Handlers must match the signal arguments and return `bool`: `true` consumes the
event; `false` continues delivery. Dispatch is synchronous and newest-first.
`connect()` returns an identifier local to that signal; use `disconnect()` or
`disconnect_all()` to remove handlers.

Member connections do not own their receiver, which must outlive the
connection. Do not modify a signal's connections during its emission. Use UI
signals on the UI thread unless access is externally synchronized.

Backends translate native event data to public Native types before calling
`emit()`. An optional signal initializer may defer event-source setup; it runs
once before the first `connect()` or `emit()`.

## 4. Setters and Getters

Expose mutable properties through `set_<property>(value)` and
`get_<property>()`. Getters must be `const`. Setters should return a reference
to the object when chaining is useful.

~~~cpp
const std::string &get_title() const;
app_wnd &set_title(const std::string &title);
~~~

Keep the property in portable class state. A setter must validate and cache the
new value whether or not its native resource exists. If it exists, update it
immediately; otherwise, `create()` must apply the cached value. A getter returns
the cached value without exposing or querying native types. Use a separate
backend notification to update cached state after native-originated changes and
avoid sending the change back to the backend.

## 5. Windows

`wnd` is the portable base class for top-level windows and controls. It contains
only behavior and state common to all windows: bounds, parent and children,
layout, graphics access, invalidation, lifecycle operations, and signals.
Derived types add only their specific properties and events. Native handles and
backend graphics state belong in bindings, never in public window classes.

Window lifecycle must follow these rules:

- Construction records portable state but does not create native resources.
- `create()` is idempotent, creates bindings, applies cached properties, and
  emits `on_wnd_create` once per creation.
- A child requires an assigned, created parent before it can be created.
- `show()` requires a created resource.
- `destroy()` is idempotent and releases native resources and bindings.

Parents and children do not own each other; their lifetimes must be managed by
the application. A window owns its installed layout manager. Geometry changes
must update cached bounds and relayout children. Backend resize notifications
must update the cache and layout without requesting the same resize again.

Backends implement creation, display, destruction, invalidation, painting, and
event translation with identical public behavior. Add each new window type to
every supported backend and keep all platform differences below the public API.

## 6. Painting in Windows

`gpx` is the portable, abstract drawing interface. It provides common drawing
state and virtual operations for clipping, clearing, lines, rectangles, text,
and images. Its API must use only public Native types and should return `gpx &`
from drawing and state-changing operations when chaining is useful.

Use concrete contexts for different drawing targets:

- `gpx_wnd` draws into a created window and borrows that window.
- `gpx_img` draws into an owned background image and borrows that image.
- Backend handles, renderers, buffers, and cached drawing objects remain in
  private bindings.

A native expose or paint event must be translated into one synchronous
`on_wnd_paint` emission on the UI thread. The `wnd_paint_event` contains the
invalid rectangle and a borrowed `gpx &` whose concrete context is `gpx_wnd`.
The context is valid only during the callback; handlers must not store or
delete it. Painting must honor the invalid rectangle and active clip.

`invalidate()` only schedules a repaint. It must not emit the paint signal
directly. Each backend is responsible for preparing its context, presenting
buffered output when required, and releasing all graphics resources during
window destruction.

## 7. Custom Drawing

Use the public `theme` drawing facade when custom controls or visuals must
match the active platform. It accepts a borrowed `gpx &` and exposes the same
semantic primitives and states on every backend, including common button,
menu, selection, border, text, hot, pressed, selected, and disabled states.

Theme rendering follows these rules:

- Prefer native theme or toolkit functions when they can draw into the target.
  Examples include Windows theme drawing, Motif `XmeDraw*` primitives, and
  equivalent AppKit or BeAPI facilities.
- Fall back to portable `gpx` operations when no suitable native primitive
  exists, or when drawing into an image rather than a native window.
- Obtain colors, fonts, spacing, and dimensions from the backend wherever
  possible; do not hard-code one platform's appearance into shared code.
- Preserve the caller's `gpx` state after drawing a theme primitive.
- Keep platform types and native calls inside the backend implementation.

Adding a theme primitive requires updating the shared interface and every
backend. A backend may report that native drawing is unavailable, but it must
provide a usable portable fallback with the same states and public behavior.
Adding a new public `wnd` subclass still requires lifecycle, event, and drawing
support in every backend as described in Section 5.

## 8. Application

Application code must define `program()` instead of an operating-system entry
point. Not every target starts through a conventional `main()`; a backend may
require `WinMain`, `main`, or another native launcher. The backend owns that
launcher, normalizes its arguments, initializes `app::argc`, `app::argv`, and
`app::envp`, then calls `program()` exactly once.

~~~cpp
int program(int argc, char **argv) {
    main_window window;
    return native::app::run(window);
}
~~~

`program()` is the portable application entry point. It must use only the
public Native API, construct the main `app_wnd`, and return a process exit code.
It must not define or call a platform entry point, start a backend event loop
directly, or depend on native argument types.

The startup classes have distinct roles:

- `app` is a static coordinator and must not be instantiated or derived from.
- `app_wnd` is the portable main-window base class. Applications normally
  derive one class from it to hold controls, state, and signal handlers.
- The application owns its main-window object. It must remain alive for the
  complete call to `app::run()`.

Constructors configure portable state and connect signals but do not create
native resources. Create child controls from `on_wnd_create`, after the main
window and its bindings exist.

`app::run()` owns the standard startup sequence:

1. Reject a second active application loop and register the borrowed main
   window.
2. Initialize shared application state and detect screens.
3. Create and show the main window.
4. Enter the backend implementation of `app::main_loop()`.
5. On exit, destroy remaining native resources, clear the main-window pointer,
   and return the backend exit code.

Application code should call `app::run()`, not `app::main_loop()`. Backends
implement only the native launcher and event loop; they must preserve this
public startup order and return the value produced by `program()` to the
operating system. `app::main_wnd()` returns a borrowed pointer only while
`app::run()` is active and must return null before and after that interval.

## 9. Screens

`screen::detect()` runs before the main window is created and replaces the
process-owned screen snapshot. Each backend reports active displays using the
same virtual coordinates as windows.

Each screen stores a contiguous index, full bounds, work area, and primary
flag. Work area excludes system UI when available and otherwise equals bounds.
Exactly one detected screen must be primary.

`count()`, `at()`, `primary()`, and `virtual_bounds()` use the cached snapshot
without native queries. Returned pointers remain valid until the next
`detect()`. No displays produce an empty snapshot; detection failures throw
`std::runtime_error`.
