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
The core ignores a notification repeating the cached dimensions and never lets
a layout pass re-enter itself, so a backend may report geometry freely,
including synchronously from inside the call that applied it. A backend must
report the client size its window really has once that window and its menu
exist, so the first arrangement matches the area it renders into.
Requested top-level screen coordinates are preferences. A backend must keep
the native title area within a detected work area, even when the requested
window is taller than that work area.

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
Motif `FileSelectionBox`, XView `File_chooser`, WINGs `WMOpenPanel` and
`WMSavePanel`, and GEM AES file selector are the standard paths. Toolkits
without a chooser, including Athena and SDL2, may delegate to an installed
desktop chooser. A backend fallback must be composed from that toolkit's
native controls rather than custom-painted substitutes. Unsupported
native options may degrade conservatively: an older single-selection chooser
may return one path, and a platform may keep mandatory overwrite confirmation.
If neither a delegated chooser nor a native-control fallback is available, the
panel must complete as cancelled and restore its owner's input state; runtime
capability absence is not an exception.

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
`radio`, `list`, `text_edit`, `code_edit`, `accordion`, `icon_view`,
`tree_view`, and `table_view` must use the platform or toolkit's native
control when one exists. A backend without a widget set may emulate the
control through its own theme and event loop. Programmatic property setters
update cached/native state without emitting user-action signals;
backend-originated changes update the cache and emit the corresponding signal.
Sibling `radio` controls are mutually exclusive, and `list` is
single-selection with `-1` representing no selection. Keep every control in
its own same-named public, common, and backend source file. Do not collect
unrelated controls into a `controls` module or add a `_box` suffix to the
`check`, `radio`, or `list` type names.

`accordion` owns section state but borrows its section content windows. Its
default single mode leaves all headers visible and assigns remaining height to
the expanded body; multiple mode keeps independent expanded states.
`icon_view` remains distinct from the text-only `list`: it owns shared image
references and item values, wraps image-and-label tiles, scrolls internally,
and exposes single selection plus activation. Windows maps it to
`WC_LISTVIEW`; macOS maps it to `NSCollectionView`, and macOS composes the
accordion from AppKit stack/disclosure controls. A backend lacking an
appropriate widget must provide keyboard-focusable input and compose its own
native theme resources rather than imitate another platform.

The OPEN LOOK backend uses XView Panel items and OpenMenu objects for its
interactive controls and menus. It must not replace an available XView widget
with a custom-painted substitute. Custom OPEN LOOK visuals use OLGX and the
active Panel color map so their font, geometry, colors, and state match those
native items.

The Window Maker backend uses WINGs windows, pull-down buttons, command,
switch, and radio buttons, lists, text fields, text views, and file panels.
It must not replace an available WINGs widget with a custom-painted
substitute. Custom Window Maker visuals use WINGs relief drawing, screen
colors, fonts, and indicator pixmaps so they track the active WINGs resources.

## 6. Painting in Windows

`gpx` is the portable, abstract drawing interface. It provides common drawing
state and operations for clipping, clearing, lines, rectangles, ellipses,
polylines, polygons, bounded text, and cropped or scaled images. Its API must
use only public Native types and should return `gpx &`
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
`save_state()` returns a move-only guard that restores ink, paper, pen, font,
and clip. Nearest image scaling must retain exact samples; linear scaling must
preserve source alpha. Bounded text must clip and support logical horizontal
alignment, vertical alignment, and end ellipsis without exposing backend text
layout types.

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
button, check, radio, list, editable-text frame, menu, semantic surface,
selection, focus, disclosure, separator, scrollbar, text, hot, pressed,
selected, disabled, focused, and active states. Appearance logic must live
in the platform or toolkit implementation, not in the backend-neutral library
root.

Theme rendering follows these rules:

- Prefer native theme or toolkit functions when they can draw into the target.
  Examples include Windows theme drawing, Motif `XmeDraw*` primitives,
  XView's OLGX primitives, and WINGs `W_DrawRelief` and indicator resources,
  as well as equivalent AppKit or BeAPI facilities.
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

A toolkit action callback may run while the toolkit dispatcher still borrows
the emitting widget. Such a backend must queue portable signals and lifecycle
actions until the native dispatcher returns. In particular, WINGs callbacks
must not let application code destroy the active view inside `WMHandleEvent`.
The backend releases or invalidates any callback binding before a deferred
signal can destroy its portable or native object.

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

## 11. Clipboard

`clipboard` is the portable, process-facing stream for exchanging data with
the system clipboard. It must be declared in `include/native/clipboard.h`,
implemented at the common and backend levels, and included by `<native.h>`.
Its public interface contains only Native and standard C++ types. Native
clipboard handles, atoms, pasteboards, messages, locks, and retained provider
objects belong in private backend bindings.

A clipboard stream is opened for either reading or writing and is move-only.
A read stream represents one consistent snapshot of the advertised system
formats. A write stream stages one or more representations and publishes them
only when explicitly committed. Destroying an uncommitted write stream must
leave the previous system clipboard unchanged. This transaction boundary is
required because many systems replace every existing representation when new
clipboard content is published.

The stream is typed rather than an unstructured sequence of native bytes. It
must enumerate available `clipboard_format` values, report whether a format is
present, and provide typed read and write operations. The minimum portable
formats are:

- `text`, represented publicly as valid UTF-8. Backends convert native Unicode
  encodings and platform line endings at the boundary. Portable text uses
  `\n`; an embedded null is not portable clipboard text and must be rejected.
- `image`, represented publicly as an owned `img` in top-to-bottom RGBA order.
  Alpha must be preserved whenever the native clipboard supports it. A backend
  may publish PNG and a native bitmap representation together to maximize
  interoperability, but both describe the same image.

Reading an unavailable format is a normal condition and must be distinguishable
from a backend failure. Malformed external data, invalid UTF-8, impossible
dimensions, integer overflow, and unreasonable allocation sizes must be
rejected before constructing a public value. A failed read or commit must not
leak a native resource or leave a clipboard lock held. Convenience operations
may transfer a complete string or image, while the underlying stream must also
permit bounded byte transfer so large or future formats do not require an
additional unbounded copy.

One clipboard item may advertise several formats. Format preference is
deterministic: text readers prefer the system's Unicode representation, and
image readers prefer a lossless representation with alpha before falling back
to another native bitmap representation. Backends must not return an arbitrary
format based on enumeration order. Unknown native formats may be ignored; a
future portable custom-data API must identify them with stable MIME-like names
rather than platform numeric identifiers.

Clipboard access belongs on the UI thread unless a backend explicitly
documents a safe marshaling path. A native lock or borrowed data pointer must
never remain live while an application signal or callback runs. Read data is
copied into portable ownership before returning to application code. For a
system with delayed rendering or selection ownership, the backend retains a
portable copy of committed representations until ownership is lost or the
application shuts down.

Backends use the standard system mechanism:

- Windows uses the system clipboard and publishes interoperable Unicode text
  and bitmap/image formats.
- macOS uses the general AppKit pasteboard.
- Haiku uses `BClipboard` and Translation Kit-compatible image data.
- X11 toolkits implement the `CLIPBOARD` selection, advertise `TARGETS`, and
  serve Unicode text and image targets. The `PRIMARY` selection is separate
  and must not silently replace the portable clipboard. XView backends use
  its Selection package, and WINGs backends use WINGs selection handlers,
  while preserving those X11 targets and ownership rules.
- SDL2 may use SDL's clipboard support where it is complete and must use the
  active platform service or a suitable foreign library for unsupported image
  transfer.
- GEM uses the AES scrap mechanism, publishes conventional `SCRAP.TXT` and
  monochrome `SCRAP.IMG`, and may additionally publish `SCRAP.PNG` to retain
  the portable RGBA representation. Reads prefer the lossless PNG form and
  fall back to the standard IMG form.

X11 selection conversion and other asynchronous native APIs must integrate
with the existing backend event dispatcher. They must not start a second
portable event loop. If acquisition cannot complete immediately, the backend
may complete the read through a clipboard signal or callback, but synchronous
and asynchronous backends must expose the same snapshot, format preference,
ownership, and error rules.

Every control for which clipboard operations are meaningful must use this
shared service rather than calling a backend clipboard API directly. Text-edit
controls must implement copy, cut, and paste for the current selection, use the
platform's conventional keyboard shortcuts, and preserve Unicode text. Copy is
allowed for a read-only control; cut and paste are not. Cut removes the selected
content only after the clipboard commit succeeds, and paste changes the control
only after input has been validated. Image editors and image-selection controls
must similarly copy and paste the portable `img` representation. Programmatic
clipboard operations must follow the same change-signal rules as the
corresponding programmatic control edits.

Tests must cover Unicode text, line-ending conversion, empty and unavailable
formats, RGBA image round trips, alpha preservation, multi-format commits,
failed commits preserving old content, ownership loss, and control-level
copy/cut/paste behavior. Each hosted backend also needs an integration test
that writes through one clipboard object and reads the resulting system data
through another.

## 12. Text editing

`text_edit` is the portable editable-text window and is declared in
`include/native/text_edit.h`. It is a `wnd`, participates in the normal child
window lifecycle and layout, and uses a native text control whenever the
active system or toolkit supplies one. Its public state contains only UTF-8
text, an immutable editing mode, read-only state, and a portable validator.
Native text buffers, delegates, callback records, selection positions, and
emulated cursor state remain in backend bindings.

The construction-time `text_edit_mode` is either `single_line` or
`multi_line`. Single-line values contain no line breaks. Multiline values use
only `\n`; carriage returns are native boundary details and never enter the
portable cache. Both modes require canonical scalar-value UTF-8 without an
embedded null. Text returned by `get_text()` is the last accepted complete
value, not a borrowed native buffer.

An optional `text_validator` receives the complete proposed UTF-8 value for
every user-originated insertion, deletion, replacement, and paste. It returns
true to accept that value and false to reject it. A backend with a pre-change
hook rejects the native edit before it is applied. A backend with only a
post-change notification restores the last accepted cache before application
signals run. The validator is synchronous on the UI thread, should be fast and
side-effect-free, and must never receive malformed or mode-incompatible text.
Installing a validator that rejects the current value is an error.

`set_text()` validates and applies a programmatic value without emitting
`on_change`. A validated user or native edit updates the portable cache first
and then emits `on_change` once with the complete new value. Rejected edits do
not change the cache and do not emit. Read-only state disables insertion,
deletion, cut, and paste, while selection and copy remain available.

Selection and clipboard behavior is consistent across backends. `copy()`,
`cut()`, `paste()`, and `select_all()` expose direct operations. The native or
emulated key dispatcher routes the platform's conventional Ctrl/Command
shortcuts through those same operations, so keyboard paste cannot bypass
validation. A clipboard failure is not reported as a text change. Cursor and
selection offsets may use a backend's native units privately, but conversion
to the portable cache must preserve complete Unicode scalar boundaries.

Backend controls are selected as follows:

- Windows uses the standard `EDIT` control.
- macOS uses `NSTextField` and scrollable `NSTextView` controls.
- Haiku uses `BTextView`, with a `BScrollView` for multiline editing.
- X11/Athena uses `AsciiText`, and OpenMotif uses `XmTextField`/`XmText`.
- OPEN LOOK/XView uses native `PANEL_TEXT` and `PANEL_MULTILINE_TEXT` items.
- Window Maker/WINGs uses native `WMTextField` and `WMText` widgets.
- SDL2 and GEMix keep cursor, selection, focus, and scrolling in private
  bindings and draw through their backend-owned native-look facilities.

Tests must cover both modes, Unicode cursor boundaries, selection replacement,
read-only behavior, programmatic and native change-signal rules, live
validation of typing and paste, direct clipboard functions, standard keyboard
shortcuts, and rejected-edit cache preservation.

## 13. Advanced tables

`table_view` is the model-backed, multi-column collection control. It remains
separate from the owned, single-column, text-only `list`. Its public interface
uses stable non-zero row IDs, group IDs, semantic column IDs, UTF-8 strings,
portable images, and Native geometry only. Platform indexes and widget objects
never become public identity.

A `table_view` borrows a `table_model`; the model must outlive the view or be
detached first. The model supplies its logical row count, stable row IDs, and
cells lazily. `table_store` is the materialized convenience implementation,
not the storage architecture of the control. Backends must not build one
native object per row for a virtual model. Visible-row mapping is compact in
the number of groups, so a million-row model does not require a million-row
mapping array.

Columns have stable IDs and cache title, width constraints, visibility,
alignment, image, resize, reorder, and sort capabilities. Group ranges are
ordered, disjoint logical row ranges. Expansion is view state: collapsing a
group hides display rows without removing logical rows, stable selection, or
search results. `find_and_reveal()` may expand a group to reveal the match.

Programmatic model, selection, sort-indicator, column, group, and scroll
changes update native state without emitting user-action signals. Native
selection, activation, sort, resize, reorder, and disclosure actions update
the portable cache first and emit exactly one corresponding signal. Model
notifications preserve selection and scroll anchors by stable ID when those
rows still exist.

Windows uses report-mode `WC_LISTVIEW`, `LVS_OWNERDATA` for virtual data, and
native ListView groups for explicitly materialized data. macOS uses
`NSTableView`. OpenMotif uses `XmContainer` detail view for explicitly
materialized data and its native-look compact host for virtual data. Haiku
uses its Open Tracker-licensed `BColumnListView` library for materialized data
and a `BControlLook` viewport host for virtual data. Athena, XView, WINGs,
SDL2, and GEM use their own toolkit host, focus path, theme resources, and
native-looking semantic table painter where no adequate table widget exists.

Search supports exact, prefix, and substring matching, optional case folding,
selected columns, start position, and wrapping. The default implementation
scans lazily and performs deterministic Unicode scalar matching. Large or
indexed models should override `find()` while preserving the same result
contract. Tests must cover stable selection, groups, model notifications,
search modes and Unicode, signal rules, backend lifecycle, and a million-row
model that proves cells are requested only for visible rows.

## 14. Source editing

`code_edit` is the portable source editor and extends the overlapping
`text_edit` contract. Its private document contains canonical UTF-8 with `\n`
line endings. Every public position and span is a UTF-8 byte offset on a
scalar boundary; lines are zero based. Tabs remain bytes in the document, and
`tab_width` affects presentation only. Invalid UTF-8 read from disk is
replaced with U+FFFD and exposed through a load warning.

File translation happens only in `load()` and `save()`. A load strips and
remembers a UTF-8 BOM, detects LF, CRLF, or CR endings, and normalizes the
buffer. A save restores the selected line ending and preserves a loaded BOM
when requested. Syntax styles, diagnostics, markers, selection, caret,
scrolling, and completion items never become file bytes. Optional session
JSON is application state; Native does not define or access a sidecar file.

All mutations pass through insert, erase, or replace operations. They rebuild
the line index, remap overlays, record a document-local delta undo step, and
emit text-change signals only for accepted edits. Programmatic `set_text()`
clears undo and overlays without emitting. Marker remapping moves a mark down
when a newline is inserted before its line, keeps it on the original line
when that line is split, and removes it when its line is deleted. Style runs
remain sorted, non-overlapping half-open spans.

The library paints the marker and line-number gutter on every backend. A
backend may combine that gutter with a native multiline widget when it can
preserve the portable document and overlays; otherwise it provides a
keyboard-focusable, toolkit-themed painted editor through its normal event
loop. The shared painted path renders visible source rows, current-line and
selection surfaces, style runs, diagnostic marks, caret, and a themed
completion list. Platform input translation must preserve scalar boundaries
and route standard clipboard and undo keys through the public commands.

Lexing and language intelligence are application services. `code_lexer`
returns validated `style_run` overlays for a dirty byte range; an exception
falls back to the default style without making the buffer uneditable.
Completion items are application-supplied and Up/Down, Enter, and Escape
navigate, accept, and dismiss the overlay. A gutter click reports a line; the
application decides whether that action changes a breakpoint. Native does not
embed a language server, debugger, third-party editor, RTF, or HTML buffer.

Tests must cover canonical lines, Unicode boundaries, load/save translation,
malformed-input repair, undo/redo, marker and overlay remapping, cached public
properties, signals, completion commands, and live create/show/destroy on
each supported backend.

## 15. Classic trees

`tree_view` is the owned, single-selection hierarchy control. It remains
separate from the flat text-only `list`, spatial `icon_view`, and model-backed
multi-column `table_view`. Each `tree_view_item` owns its descendant values,
retains an optional `shared_ptr<const img>`, and has a unique non-zero stable
`tree_item_id`. Native row indexes, pointers, item handles, and toolkit object
identity never cross the public API.

Expansion is view state stored on each item. Flattened visible rows contain a
stable ID and depth, and include descendants only while every ancestor is
expanded. Collapsing a branch containing the selected descendant moves the
selection to that branch, so keyboard focus never remains on a hidden row.
Replacing items preserves selection by stable ID when that item still exists;
removing a branch removes all descendants and clears descendant selection.

Programmatic item, selection, expansion, image-size, line-visibility, and
scroll changes update a created native control without emitting action
signals. Backend selection, disclosure, and activation update the portable
cache first and emit exactly one `on_selection_change`, `on_expanded_change`,
or `on_item_activate` signal. Disabled items remain visible but cannot be
selected, expanded by user input, or activated.

Up and Down move over visible enabled rows; Home and End choose enabled
endpoints; Page Up and Page Down move by a viewport. Left collapses an open
branch or selects its parent. Right expands a closed branch or selects its
first enabled child. Space toggles a branch and Enter activates it. A classic
row double click performs the platform's branch action and emits activation.
Pointer disclosure hit testing is distinct from row selection.

Windows uses `WC_TREEVIEW`, macOS uses `NSOutlineView`, OpenMotif uses
`XmContainer` outline layout, and Haiku uses `BOutlineListView`. Athena,
XView, WINGs, SDL2, and GEM use their toolkit-owned focus and event paths with
the shared semantic selection, disclosure, focus, connector, image, and
scrollbar painter because they have no adequate interactive outline widget.
Every path uses its backend theme metrics and native drawing resources.

Tests must cover unique IDs, recursive ownership, visible flattening,
selection preservation and removal, disclosure hit testing, disabled rows,
classic keyboard navigation, scrolling, programmatic silence, exact
user-action signal counts, and live create/show/destroy on every backend.
