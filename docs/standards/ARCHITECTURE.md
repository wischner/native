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

Top-level ownership is a second relationship and must never be represented by
`wnd::set_parent()`. An `owned_wnd` borrows an `app_wnd` owner but remains a
top-level window with screen-coordinate bounds. It is not added to the owner's
child list, is not clipped to the owner's client rectangle, and never
participates in the owner's layout. Destroying an owner's native resource must
first destroy the native resources of its owned windows. Destroying either C++
object must safely detach the non-owning relationship.

`modeless_wnd` and `modal_wnd` are the two portable owned-window bases:

- A `modeless_wnd` uses the backend's owned, transient, floating-subset, or
  equivalent top-level relationship. It remains in the normal application
  event loop and does not disable its owner.
- A `modal_wnd` is an owner-modal dialog. Showing it starts a modal session,
  moves focus to the dialog, and prevents input to its owner and the owner's
  other owned branches. The event loop must continue to dispatch paint and
  lifecycle events; modality must not require a second portable event loop.
  Closing it supplies an accepted or cancelled `dialog_result`, ends the
  session exactly once, and restores the previous eligible owner or modal
  dialog.

Modal sessions form a stack so a modal dialog may own another modal dialog.
Only the active modal branch accepts user input. Native owner disabling,
exclusive grabs, modal-subset window feels, and toolkit modal-parent APIs are
preferred. A backend without such a facility must enforce the same rule in its
event dispatcher and keep the active modal window above blocked windows.

All owner-modal system interactions follow the same contract. File-open,
file-save, folder selection, print, page setup, color, font, message, and
similar dialogs must derive from `modal_wnd` or adapt their native panel to one
`modal_wnd` session. They must not introduce a separate ownership, focus,
result, or event-loop policy.

`file_dialog` is the shared system-panel base for `open_file_dialog` and
`save_file_dialog`. It caches an initial path, ordered `file_filter` groups,
and the selected UTF-8 filesystem paths. `open_file_dialog` adds optional
multiple selection. `save_file_dialog` adds a suggested leaf name, default
extension, and overwrite-confirmation preference. These public classes contain
no native panel handles and do not pretend that a system panel has drawable
window geometry.

Backends must use the operating system or toolkit file selector when one
exists. The Windows Common Item Dialog, AppKit panels, Haiku `BFilePanel`,
Motif `FileSelectionBox`, and GEM AES file selector are the standard paths.
Toolkits without a chooser, including Athena and SDL2, may delegate to an
installed desktop chooser rather than implement a custom selector. Unsupported
native options may degrade conservatively: an older single-selection chooser
may return one path, and a platform may keep mandatory overwrite confirmation.

System panels may complete synchronously or asynchronously. Portable code must
observe `on_modal_close` and read paths after an accepted result instead of
depending on whether `show()` has returned. Native completion and cancellation
must end the modal session exactly once, ignore stale callbacks after explicit
destruction, and release the panel binding before application callbacks can
destroy the C++ dialog object.

Backends implement creation, display, destruction, invalidation, painting, and
event translation with identical public behavior. Add each new window type to
every supported backend and keep all platform differences below the public API.

Interactive controls are windows, not theme drawings. `button`, `check`,
`radio`, and `list` must use the platform or toolkit's native control when one
exists. A backend without a widget set may emulate the control through its own
theme and event loop. Programmatic property setters update cached/native state
without emitting user-action signals; backend-originated changes update the
cache and emit the corresponding signal. Sibling `radio` controls are mutually
exclusive, and `list` is single-selection with `-1` representing no selection.
Keep every control in its own same-named public, common, and backend source
file. Do not collect unrelated controls into a `controls` module or add a
`_box` suffix to the `check`, `radio`, or `list` type names.

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

Both context types implement the complete `gpx` contract on every backend.
Their initial clip is the complete target bounds; paint dispatch replaces a
window context's clip with the invalid region. Selecting an explicit font must
affect drawing and measurement in the same way for both targets.
`draw_text()` interprets its position as the top-left of the text line; native
APIs that accept a baseline must add the selected font's ascent internally.

`img` owns pixels in top-to-bottom RGBA order and provides PNG and JPEG file
and memory I/O. Decoding detects the format from the encoded signature rather
than a filename. Encoding selects the format explicitly for memory output and
from `.png`, `.jpg`, or `.jpeg` for file output. PNG preserves RGBA data; JPEG
discards alpha and decodes as opaque. Backends use operating-system codecs
where those are part of the platform and use shared image libraries where the
window system has no codec service. Codec handles and foreign-library types
must remain below the public API.

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

Use the public abstract `theme` interface when custom controls or visuals must
match the active platform. `theme::create()` asks the active backend for a
short-lived implementation around a borrowed `gpx &`. The interface exposes
the same semantic primitives and states on every backend, including common
button, check, radio, list, menu, selection, border, text, hot, pressed,
selected, and disabled states. Appearance logic must live in the platform or
toolkit implementation, not in the backend-neutral library root.

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
- `app_wnd` is the portable top-level application-window base. The unowned
  instance passed to `app::run()` is the main window; `modeless_wnd` and
  `modal_wnd` derive from it through `owned_wnd`.
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

## 10. Fonts

Fonts belong to one of two categories: stock fonts supplied by the active
platform or toolkit, and portable TrueType fonts supplied by the application.
The distinction is part of the public contract. A backend must not silently
replace a portable font with a stock font or treat an arbitrary platform font
as portable.

Stock fonts provide the native look and feel. At minimum, every backend must
provide semantic roles for:

- the general system font;
- the system fixed-pitch font;
- the system icon-label font;
- title text;
- small secondary text; and
- controls and menus.

These roles are requests for meaning, not for a particular family name. A
backend obtains the closest corresponding font from the operating system,
desktop, or toolkit and may map several roles to the same native font when no
distinct role exists. Stock fonts are therefore expected to differ between
backends and machines. They are process-owned, returned as borrowed
references, and must remain valid for the lifetime of the initialized backend.
Application code must not destroy or modify them.

All non-stock fonts are portable TrueType fonts. The application identifies
the exact font resource and face rather than only naming a locally installed
family. Every backend must render from those same font bytes. It may use a
native TrueType implementation when that implementation preserves the
portable behavior; otherwise it must use a shared font library. It must not
search host-installed fonts, substitute another family, or fall back to a
stock font when the requested resource cannot be loaded.

The public font API must provide functions that enumerate fonts installed on
the current system. Enumeration returns portable descriptions such as family,
style, weight, and available face names; it must not expose platform handles
or toolkit types. The result is inherently machine-specific and is intended
for font pickers and other discovery interfaces. Selecting an enumerated font
does not make it portable: applications that require identical rendering must
still supply the exact TrueType resource.

Portable fonts must be creatable from either a file or an in-memory byte
buffer. Both creation paths have the same face-selection, sizing, validation,
and rendering behavior. File creation reads the resource during creation and
must not depend on the file remaining open afterward. Memory creation copies
or otherwise owns the bytes needed for the complete lifetime of the font; it
must never retain a borrowed pointer to application memory. Collections must
allow an explicit face index in both creation paths.

For a portable font, the following behavior must be backend-independent:

- UTF-8 decoding and missing-glyph handling;
- face selection, size, weight, and style;
- glyph advances, kerning, line metrics, and text bounds; and
- the placement and alpha coverage produced by `gpx::draw_text()` for the same
  text, font data, graphics state, and scale.

Every font exposes editor-oriented measurements without requiring a drawing
operation. `font_metrics` reports positive ascent, descent, leading, total
line height, and maximum character advance in pixels. `text_metrics` reports
the visible width, line height, and cursor advance of a UTF-8 string.
`measure_character()` accepts one Unicode scalar value; surrogate values and
values above `U+10FFFF` are invalid. An empty string has zero width and advance
but retains the selected font's line height.

The same measurements are available through `gpx`, where they always use the
currently selected font. This is the canonical path for editor layout: line
spacing comes from `get_font_metrics()`, cursor movement comes from
`measure_character().advance`, and selection or run bounds come from
`measure_text()`. A backend must use the same native face and shaping or
rasterization path for measurement that it uses for `draw_text()`.

Malformed data, an unsupported TrueType face, an invalid face index, or an
invalid size produces an invalid `font_t`; drawing with an invalid explicitly
selected font is a no-op. Failure must be observable through `valid()` and
must not change the selected font or leak a partially created native resource.

`font_t` owns non-stock font registrations and remains move-only. Its public
description contains only portable values; parsed font data, rasterizer state,
and native handles remain in private shared or backend bindings. Moving a font
preserves its opaque identifier and rendering behavior. Destroying it releases
all associated registrations and cached glyph resources. A `gpx` borrows its
selected font, so that font must outlive every drawing operation that uses it.
