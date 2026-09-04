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
- Implement library-owned path manipulation, traversal, metadata, rename,
  and removal with C++20 `std::filesystem`. Read and write file contents with
  standard C++ streams. Do not use direct POSIX or C filesystem calls, shell
  command construction, or OS-specific helpers where the standard library
  provides the operation. Public values that identify filesystem entries use
  `std::filesystem::path`; filename patterns, extensions, and labels remain
  text. Descriptor, process, signal, and terminal work continues to use POSIX
  interfaces. Native window, control, standard-panel, system-icon, and
  known-folder APIs remain permitted inside their platform or toolkit layer
  because the C++ standard library has no equivalent.
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

Every created `wnd` owns one opaque `detail::wnd_peer`. The peer routes common
geometry, visibility, and invalidation operations and owns typed backend state
without placing a toolkit type in a public header. Object-to-state lookups use
the stateless internal `detail::peer_bindings` adapter and therefore require no
process-wide object-to-state registry.

The `native::bindings<A, B>` class remains the bidirectional mapping for a
native callback handle or resource identifier that must recover a library
object.

Typical mappings include:

- A native window handle and its `native::wnd *` object.
- A font or menu identifier and its backend resource.

This lets the backend:

- Find the `wnd` that owns a native event.
- Find a callback-facing native handle associated with a given `wnd` when the
  backend cannot retain that handle in peer state.
- Map non-window resource identifiers without exposing toolkit types.

Declare callback-facing bindings in a private `globals.h` file and define them
in the corresponding `globals.cpp` file. Place them in the platform namespace
when a platform does not use a toolkit. When it does, place them in a nested
`platform::toolkit` namespace. Do not add one `native::bindings` instance per
control type; state that belongs to one window belongs to its peer.

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

`connect_scoped()` returns a move-only `native::connection`. The handle
disconnects its slot when destroyed and may safely outlive the signal; call
`release()` only when the receiver's lifetime is managed another way and the
slot must remain connected. Prefer scoped connections for callbacks that
capture an object or bind one of its member functions.

Member connections do not own their receiver, which must outlive the
connection. Do not modify a signal's connections during its emission. Use UI
signals on the UI thread unless access is externally synchronized.

Backends translate native event data to public Native types and call the
control's corresponding virtual `on_native_*` entry point. They must not emit
a control signal directly. The base virtual method validates and updates the
portable cache before emitting the public signal. A derived control may
override the entry point to add behavior and call the base implementation to
retain the normal state transition and notification. Internal semantic events
that are not direct toolkit callbacks follow the same template-method pattern
through a protected virtual hook. An optional signal initializer may defer
event-source setup; it runs once before the first `connect()` or `emit()`.

Keyboard focus follows the same route: backends call `wnd::on_native_focus()`
without switching on the public control type. The `wnd` default is a no-op;
`custom_control` caches focus and invalidates, and specialized controls may
extend that behavior.

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
layout, mouse cursor, graphics access, invalidation, lifecycle operations, and
signals.
Derived types add only their specific properties and events. Native handles and
backend graphics state belong in the internal peer or callback-facing
bindings, never in public window classes.

Window lifecycle must follow these rules:

- Construction records portable state but does not create native resources.
- `create()`, `show()`, `destroy()`, and `invalidate()` are mutating,
  non-`const` operations.
- Public `create()`, `show()`, and `destroy()` are non-virtual template
  methods. Backends implement only `create_native()`, `show_native()`, and
  `destroy_native()`.
- `create()` is idempotent, validates the parent once, establishes the backend
  resource, applies the cached cursor, marks it created, and emits
  `on_wnd_create` once per creation. A failed backend hook restores the
  uncreated state.
- A child requires an assigned, created parent before it can be created.
- `show()` requires a created resource and makes `get_visible()` true only
  after the backend hook succeeds. A synchronous system panel may accept or
  cancel and destroy its logical resource inside that hook; in that case
  `show()` returns with both created and visible state false and must not apply
  any more backend state to the completed resource.
- `destroy()` is idempotent and releases native resources and bindings.
- Backend-owned drawing resources must be released before the native window
  or surface they target. In particular, SDL renderers are destroyed before
  their `SDL_Window`; process-wide SDL shutdown belongs to the application
  loop, never to a closing modeless window.
- A window-manager close request must converge on `destroy()`, not merely hide
  or dismiss the native resource. A later `create()`/`show()` therefore starts
  a complete new native lifecycle on every backend.

`mouse_cursor` exposes the portable system shapes `arrow`, `ibeam`,
`crosshair`, `resize_horizontal`, `resize_vertical`,
`resize_northwest_southeast`, and `resize_northeast_southwest`. Every `wnd`
caches its choice; `set_cursor()` is valid before or after creation, and
`get_cursor()` returns that cached value. Windows default to `arrow`, while
text editors select `ibeam` during construction. Backends apply the cache
through the protected `apply_cursor()` hook after creation, again after showing
a resource that may only then be realized, and immediately after a live value
changes. Emulated child-window backends must choose the cursor of the deepest
visible child under the pointer. A backend without a corresponding directional
system cursor must use its precision-pointer fallback rather than an unrelated
arrow.

Parents and children do not own each other; their lifetimes must be managed by
the application. A window owns its installed layout manager. Geometry changes
must update cached bounds and relayout children.

The layout region is derived in two documented steps. `get_chrome_bounds()` is
a protected virtual returning the host-relative area a window leaves for
non-client elements and the client after its own permanent edge furniture; the
base returns the complete bounds, so a plain window reserves nothing.
`get_client_bounds()` is that area further reduced by every visible non-client
element, and non-client strips are placed inside the same chrome rectangle.
A control that owns edge chrome, such as canvas scrollbars, overrides only the
first step, and both the client area and the strip geometry stay consistent.
Backend resize notifications must update the cache and layout without
requesting the same resize again.
Portable layout may temporarily assign a child zero width or zero height. A
backend whose native window system requires non-zero dimensions must use its
minimum native backing size without changing the cached portable bounds.
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
  event loop, accepts controls through that loop, and does not disable its
  owner.
- A `modal_wnd` is an owner-modal dialog. Showing it starts a modal session,
  moves focus to the dialog, and prevents input to its owner and the owner's
  other owned branches. The event loop must continue to dispatch paint and
  lifecycle events; modality must not require a second portable event loop.
  Closing it supplies an accepted or cancelled `dialog_result`, ends the
  session exactly once, and restores the previous eligible owner or modal
  dialog.

Closing any owned top-level resource must release pointer capture and restore
keyboard focus to the previous eligible application window. This applies to
modeless windows and synchronous platform panels as well as `modal_wnd`; the
first control click after closure must be delivered as an action rather than
consumed only to reacquire focus.
An activation click from outside the application follows the same rule. SDL2
sets `SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH` before creating a window so the
window-manager activation click reaches the control under the pointer. If a
window manager still consumes the press but delivers its release, SDL2
reconstructs that single press/release transition once after focus gain.

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
and the selected `std::filesystem::path` values. `open_file_dialog` adds
optional multiple selection. `save_file_dialog` adds a suggested leaf name,
default extension, and overwrite-confirmation preference. These public
classes contain no native panel handles and do not pretend that a system
panel has drawable window geometry.

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

Painted controls use the public `custom_control` base for shared focus state
and active-theme metric synchronization. Indexed painted collections use its
`collection_view` subclass. That base also owns the optional pixel-based
vertical scroll offset used by `icon_view` and `tree_view`; collections with a
different scrolling model retain their specialized policy. `accordion`,
`icon_view`, `tree_view`, and `table_view` derive from `collection_view`;
`canvas` derives from `custom_control`. These bases expose no backend handles
or toolkit types.

Interactive controls are windows, not theme drawings. `button`, `check`,
`radio`, `list`, `text_edit`, `code_edit`, `accordion`, `tab_view`, `icon_view`,
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

Item collections retain their named `add_item()` or `add_column()` operation
and additionally provide `operator<<` as append-only builder sugar. The
operator must never configure a property, and it must never be the only way to
perform an append. Descriptor values such as `tab_page`,
`accordion_section`, and `tree_node()` make borrowed ownership and stable
identity explicit at the call site.

`accordion` owns section state but borrows its section content windows. Its
default single mode leaves all headers visible and assigns remaining height to
the expanded body; multiple mode keeps independent expanded states.
Its complete outer border is visible by default and can be changed with
`set_border_visible()`. A visible border insets borrowed body geometry so a
repainted child cannot cover the accordion's left, right, or bottom edge.
`tab_view` follows the same borrowing rule: it owns stable `tab_item`
descriptors but never owns the page windows. Only the selected page is
created. Programmatic selection is silent and only native user selection emits
`on_selection_change`. Backends use a standard native tab peer when one exists
and retain the same model and lifecycle through a themed host otherwise.
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
native items. Canvas-backed collection panels are frame siblings rather than
children of the portable composite surface; their frame position must therefore
include the complete portable ancestor offset and the top-level menu height.
They must never cover a host-owned tab strip or splitter. Invalidating a
custom collection paints into its backing Pixmap and copies the requested
region directly; it must not clear the live Panel before requesting Expose.
`icon_view`, `tree_view`, and `table_view` attach real XView `Scrollbar`
objects to that Panel (vertical where applicable and horizontal for tables).
The backend synchronizes their object, page, and view positions in both
directions and suppresses only the duplicate portable scrollbar paint; it
must not substitute an emulated track when XView supplies the native control.
Vertical XView scrollbars occupy the east-side extent reserved by the shared
renderer; a table scrollbar begins below its header, and a horizontal bar
occupies the reserved bottom extent without overlapping the corner. OPEN LOOK
alternating table rows use the active control CMS background and highlight
colors rather than hard-coded RGB values.
OPEN LOOK compact captions use rounded button geometry and the Panel
control colors, never inverse selection colors or the white 3-D OLGX edge.
The close mark is drawn directly on that caption fill and must not invoke the
ordinary button-background painter. Pin states use the OLGX menu pushpin
glyph. Disclosure and sort marks use OLGX's reported menu-mark dimensions to
center the glyph inside their semantic slot. Tree connector strokes stop
before that slot, and a collapsed branch has no downward child stem.

An OPEN LOOK owned window is an actual XView subframe of its portable owner,
not a second root frame. A window-manager dismiss destroys only that subframe
and clears its portable created state; reopening creates a fresh subframe.
Only the main application window is created as an XView root frame.

The Window Maker backend uses WINGs windows, command, switch, and radio
buttons, lists, text fields, text views, file panels, and scrollers. It must
not replace an available WINGs widget with a custom-painted substitute.
Custom Window Maker visuals use WINGs relief drawing, screen colors, fonts,
and indicator pixmaps. The reference session normalizes the WINGs panel gray
to the desktop's `#AAAAAA` inactive-title color; only editable text/document
surfaces are white. Tables use the session's `#D7D7D7` body, `#C8C8C8`
alternate row, `#555555` selection, and `#808080` header roles. Table,
accordion, collection, and table headers share the same compact edge recipe:
white on the left only, black on top, and dark on the right and bottom.

Window Maker application menus are click-persistent context-style popups, not
press-drag WINGs selector buttons. Portable menu text supplies a mnemonic with
`&`, a literal ampersand with `&&`, and an accelerator label after a tab. The
backend must size the popup to both columns and support mouse dismissal,
mnemonics, arrow navigation, and accelerators. `menu_separator` is a
non-command structural entry: every backend maps it to its native separator
or native-themed separator row, excludes it from command dispatch, and skips
it during keyboard navigation. A popup uses a black outer box
plus a raised inner relief with white top/left and dark bottom/right edges;
menu-item painting and hit testing stay inside both frame layers. The flat
application menu strip ends with the native one-pixel dark separator before
client content; title
repaints and window resizing must preserve that rule. `icon_view`, `tree_view`,
and `table_view` attach actual WINGs scrollers and suppress only the duplicate
portable scrollbar painting. Their native child extents occupy the reserved
strips without covering table header cells or content; a table's vertical
scroller spans the complete right strip beside both header and body, while a
horizontal scroller ends before it. A Window Maker table stretches its last
visible column across otherwise-unused viewport width without changing the
column's semantic width or creating horizontal overflow.
The basic text-only `list` remains a genuine `WMList`, including its WINGs
input and scroller. Because the stock painter uses white for selection, the
backend must use `WMSetListUserDrawProc` to apply the same dark-gray selection
and light-gray selected text roles as Window Maker collections and tables;
unselected rows retain the panel gray. The Window Maker implementation of
`theme::draw_list_item` uses those identical roles so native and themed list
presentation cannot disagree.
WINGs sort-arrow pixmaps contain a resource-paper color even when their
supplied mask covers it. The backend color-keys that paper and composites only
native glyph pixels onto the semantic header surface. One-color disclosure
pixmaps cannot be safely separated from that paper on common WINGs builds, so
the backend paints the native compact filled right/down triangle directly
through `theme::draw_disclosure`; an arrow must never introduce an opaque
square background or disappear with its mask.

## 6. Painting in Windows

`gpx` is the portable, abstract drawing interface. It provides common drawing
state and operations for clipping, clearing, lines, rectangles, ellipses,
polylines, polygons, bounded text, and cropped or scaled images. Its API must
use only public Native types and should return `gpx &`
from drawing and state-changing operations when chaining is useful.

Use concrete contexts for different drawing targets:

- `gpx_wnd` draws into a created window and borrows that window.
- `gpx_img` draws into an owned background image and borrows that image.
- Per-window renderers, buffers, and cached drawing objects remain in the
  internal peer. Other target handles and caches remain in private bindings.

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
must remain below the public API. File load and save accept
`std::filesystem::path`.

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

Public controls must remain inheritable. Their semantic painting is divided
into protected virtual template-method stages. Simple controls dispatch
background, border or indicator, text/content, and focus stages from their
complete `draw_control()` entry point. Collections expose the corresponding
row/cell, image, disclosure, and scrollbar stages. `app_wnd` and `panel`
expose their background stage, and `split_view` exposes separate splitter
background and grip stages. The base implementation performs the actual
default drawing inside each virtual method. A renderer calls each virtual
stage in place; it must never paint a default part first and invoke an owner
callback afterward. This lets an override replace a part completely or call
the base implementation and add decoration without double painting.

Native-widget backends retain the toolkit's interaction, accessibility, and
metrics and use its supported owner-draw, custom-draw, cell/view, expose, or
repaint mechanism to enter the same virtual stages. The stage's base
implementation uses that backend's semantic `theme` primitives. Shared
renderers and backend adapters use an internal friend dispatcher when they
need access to protected stages; public headers never expose native handles.
Every new control event and every new painted part requires corresponding
virtual behavior and painting coverage in all backends plus a derived-control
test that overrides and calls base.

The default application and container surface is the backend's `panel` role;
editable text and item-bearing views use the contrasting `content` role.
Complete theme primitives for checks, radios, and similar controls must use
the same metrics and semantic roles as the corresponding live control's base
draw stages, so an application-painted sample is not a second visual design.

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
allow an explicit face index in both creation paths. Enumerated and selected
file resources are `std::filesystem::path` values.

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
emulated cursor state remain in the internal peer; callback handles remain in
backend bindings.

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
Emulated editors paint selected text with the same active and inactive
selection background and foreground roles used by collection controls; they
must not introduce a backend-local highlight color.

Backend controls are selected as follows:

- Windows uses the standard `EDIT` control.
- macOS uses `NSTextField` and scrollable `NSTextView` controls.
- Haiku uses `BTextView`, with a `BScrollView` for multiline editing.
- X11/Athena uses `AsciiText`, and OpenMotif uses `XmTextField`/`XmText`.
- OPEN LOOK/XView uses native `PANEL_TEXT` and `PANEL_MULTILINE_TEXT` items.
- Window Maker/WINGs uses native `WMTextField` and `WMText` widgets.
- SDL2 and GEMix keep cursor, selection, focus, and scrolling in private
  bindings and draw through their backend-owned native-look facilities. SDL2
  reclamps retained horizontal scrolling whenever editor bounds change, so a
  temporarily zero-width editor cannot reopen with its value off-screen.

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

Scrollbar visibility uses `scrollbar_policy`, declared in
`include/native/scrollbar.h` and shared with `canvas`. It is one definition
with one qualified name; `table_view.h` must not redeclare it.

The final visible column fills unused viewport width by default on every
backend. `set_fill_last_column(false)` opts out. Fitting changes presentation,
not the configured semantic column width, and horizontal overflow retains the
configured widths.

Columns have stable IDs and cache title, width constraints, visibility,
alignment, image, resize, reorder, and sort capabilities. Group ranges are
ordered, disjoint logical row ranges. Expansion is view state: collapsing a
group hides display rows without removing logical rows, stable selection, or
search results. `find_and_reveal()` may expand a group to reveal the match.

Programmatic model, selection, sort-indicator, column, group, and scroll
changes update native state without emitting user-action signals. Native
selection, activation, sort, resize, reorder, and disclosure actions update
the portable cache first and emit exactly one corresponding signal. Model
incremental notifications preserve selection and scroll anchors by stable ID
when those rows still exist. A complete reset rebuilds the visible mapping,
removes stale selection, and clamps scrolling without consulting obsolete row
indices.

Row height is a complete row/selection height, not merely text padding.
`set_row_height()` installs an explicit portable height and `std::nullopt`
restores the backend's `theme::metrics::table_row_height`. Backends choose a
native default independently from compact list-item height. The Window Maker
default includes the vertical spacing used by the reference Task Manager, so
its selection fill occupies the same taller row rather than hugging the text.
Table painting has a separate protected `draw_border()` stage invoked after
headers, rows, and portable scrollbar parts. Its base implementation draws the
backend-requested `table_outer_border_extent`; it must not be folded into the
initial background stage because edge-aligned content can overpaint it. A
library-painted table passes the complete control bounds so its right and
bottom edges include the scrollbar reservations. When the default trailing
column fitting is active, its header also covers the top-right area above a
vertical scrollbar. Native scroller hosts may retain separately framed native
controls. Window Maker uses a one-pixel black top/left and white bottom/right
relief. Column-header cells use their distinct table-header surface role.
Window Maker table and collection headers have a white left edge only; their top
edge remains black, matching the reference Task Manager rather than an
ordinary raised-button relief.

Library-painted tables distribute the final visible page over the complete
body when the viewport height is not an exact multiple of row height. Every
visible row is complete and no unpainted strip remains below the items. Their
complete outer frame is the last paint stage. A portable scrollbar includes
decrement and increment arrow buttons, a page trough, and a gripped thumb; all
parts use the active backend palette.

Pointer hit testing on a library-painted table uses the theme metrics cached
when the control was created. An emulated child may paint into its root
window and have no child `gpx`; input routing must not call `get_gpx()` on
that child merely to recover row or header geometry.

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
The remembered source location and all path-taking operations use
`std::filesystem::path`.

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
An unset `code_theme::current_line` is a native selected-row surface, and all
text fragments on that row use the corresponding selected-text role. A
backend must not place normal syntax ink on a selection color with inadequate
contrast. An explicit current-line color remains an application override and
retains syntax-specific foregrounds.

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

Programmatic item, selection, expansion, image-size, line-visibility,
border-visibility, presentation, and scroll changes update a created native control without
emitting action signals. Backend selection, disclosure, and activation update
the portable cache first and emit exactly one `on_selection_change`,
`on_expanded_change`, or `on_item_activate` signal. Disabled items remain
visible but cannot be
selected, expanded by user input, or activated.

Up and Down move over visible enabled rows; Home and End choose enabled
endpoints; Page Up and Page Down move by a viewport. Left collapses an open
branch or selects its parent. Right expands a closed branch or selects its
first enabled child. Space toggles a branch and Enter activates it. A classic
row double click performs the platform's branch action and emits activation.
Pointer disclosure hit testing is distinct from row selection.

Windows uses `WC_TREEVIEW`, macOS uses `NSOutlineView`, and Haiku uses
`BOutlineListView`. OpenMotif uses one `XmContainer` outline implementation
for both presentations. The default CDE presentation uses flat
`XmIconGadget` entries; `three_dimensional` changes native gadget relief
without switching to a second painted control. Stable IDs, portable images,
and expansion state are adapted to native gadgets, while `XmContainer` owns
selection, disclosure, keyboard interaction, layout, and scrolling. Athena,
XView, WINGs, SDL2, and GEM use their toolkit-owned focus and event paths with
the shared semantic selection, disclosure, focus, connector, image, and
scrollbar painter because they have no adequate interactive outline widget.
Every library-painted collection scrollbar uses the same classic arrow,
trough, and gripped-thumb composition as a painted table. Its arrow and page
areas scroll immediately, and a backend with pointer input must keep thumb
capture active until release so dragging remains responsive outside the
track. The flat Motif `list` is hosted by an `XmScrolledWindow`; Motif icon views,
materialized tables, and both tree presentations use `XmContainer` with its
toolkit-owned automatic scrollbars. Scrollbar value callbacks update portable
scroll state without synthesizing selection or activation.
Connector visibility remains an explicit portable tree property and defaults
off on every backend. Indentation and the platform's compact right/down
disclosure indicator express hierarchy unless an application enables classic
branches. Selected rows use the selection-text color and never carry an opaque
resource-paper rectangle on painted backends.
The complete tree border is visible by default and can be disabled with
`set_border_visible(false)`. Painted backends draw it after rows and scrollbars;
native outline backends map it to their native scroll-view or client-edge frame.
Every path uses its backend theme metrics and native drawing resources.

Tests must cover unique IDs, recursive ownership, visible flattening,
selection preservation and removal, disclosure hit testing, disabled rows,
classic keyboard navigation, scrolling, programmatic silence, exact
user-action signal counts, and live create/show/destroy on every backend.

## 16. Split views and tabs

`split_view` is a two-pane child control. It borrows exactly two uncreated
`wnd` objects, owns neither, and detaches both when destroyed. Orientation,
ratio, pane minimums, and separator extent are backend-neutral cached state.
Programmatic changes are signal-silent; a native or pointer drag updates the
ratio first and then emits one `on_ratio_change` notification.

Backends use their actual container widget where one exists: Athena Xaw
`Paned`, Haiku `BSplitView`, Motif `XmPanedWindow`, AppKit `NSSplitView`, and
Window Maker `WMSplitView`. The Xaw and Motif adapters map portable minimums
and ratios to native pane constraints and keep both panes adjustable through
native grips or sashes. Backends without a general-purpose stock splitter use
a native child host and the shared separator interaction without introducing
top-level window management.

An emulated split backend must paint the split host before its pane children,
register the divider for root-relative hit testing, and retain pointer capture
until release so dragging continues beyond the narrow divider. Horizontal
splits use the horizontal-resize cursor and vertical splits use the
vertical-resize cursor.

`tab_view` independently borrows any number of page windows and creates only
the selected page. Its top, bottom, left, and right placement remains a pure
C++ model property; side labels use directional rotation. Native
implementations use the platform tab control where its placement and label
direction satisfy that contract,
including Motif `XmNotebook`, Haiku `BTabView`, AppKit `NSTabView`, Win32 tab
common controls, and Window Maker `WMTabView`. A backend wrapper may provide a
page-local content host, but it must remain below the public API boundary.
Library-painted tabs overlap the adjoining page edge by one device pixel when
selected. The page frame or strip-only separator paints first, preventing a
gap on any of the four tab placements. When the page frame is visible, the
first tab's leading cross-axis edge must align with the corresponding page
frame edge; strip-only tabs remain flush with the complete control edge.

Split views and tabs compose normally and may be placed by any layout manager.
Neither control creates floating windows, persists layouts, draws drop targets,
or changes top-level ownership. Tests cover borrowed lifetime, minimum geometry,
orientation, programmatic silence, user event counts, and native
create/show/destroy transitions.
## 17. Input controls, standard dialogs, and non-client chrome

The public input, filesystem-resource, and chrome API consists of `combo_box`,
the `list_box` alias, `file_icon`, `special_directory`, `directory_dialog`,
`message_box`, `non_client`, `ruler`, and `status_bar`.
Headers remain pure C++ and are exported through `include/native.h`. Portable
state and fallback behavior live in `lib/native/`; native widget creation,
dialog presentation, and event routing remain in the matching platform or
toolkit directory.

`combo_box` caches its UTF-8 item model, selected index, complete displayed
text, and editable/selection-only style. Programmatic changes do not emit
user-action signals. Backends must update the cache before invoking the
virtual native event hook. The base hooks emit the public signals. A backend
with a native combo widget must use it; a toolkit without one composes its
standard text, menu, or choice widgets. Backend-owned emulation must reuse the
backend theme and input dispatcher.

An emulated combo uses a compact filled downward arrow on a content-colored
button. Its popup opens below when space permits and above otherwise, paints
over ordinary siblings, and
receives hit testing before every ordinary sibling even when their bounds
overlap. It consumes the outside click that dismisses it. A selection-only
field opens from the field or arrow; an editable field keeps ordinary text
focus when its text area is clicked and opens only from its arrow button. The
arrow-button paint must not erase the combo's outer border. SDL2's fully owner-drawn
editable combo opens from either its text or arrow while retaining text-input
focus. Its open popup tracks the pointer and highlights only the row currently
under it, returning to the selected-row highlight when the pointer leaves.

`list_box` is an alias of `list`, not a second list implementation. This keeps
selection semantics, native widgets, drawing extension points, and event
hooks identical.

`file_icon` owns one exact-size PNG byte sequence for a
`std::filesystem::path`. `from_path()` uses standard filesystem metadata to
select file or directory behavior; `for_file()` and `for_directory()` provide
an explicit type for a path that may not exist. A backend first asks its
system icon service for the path or file type, renders the result at the
requested square size, and encodes it as PNG. If no native image is available,
shared code scales its attributed generic file or folder PNG and records
that fallback in `file_icon_source`. Native handle and bitmap types never
cross the private backend boundary.

`special_directory` is a process-owned snapshot of semantic system locations
expressed as `std::filesystem::path`. `detect()` refreshes it; `count()`,
`at()`, and `find()` address the latest snapshot. Roles include home, desktop,
documents, downloads, media, public/templates, applications, fonts,
configuration, application data, cache, and temporary storage. A backend
reports only locations its operating system can identify; shared code adds
the standard temporary directory and normalizes paths without creating them.
Each entry obtains its icon through the same `file_icon` contract.

The Haiku status-strip painter follows the compact system `StatusView`
convention and uses `BControlLook` for its background. Do not substitute
`BStatusBar`: that class represents progress state and has different layout
and painting semantics. Its platform theme height aligns its top edge with the
titled-window resize marker; this is distinct from the smaller scrollbar
extent. The root Haiku canvas owns background clearing and must request a
complete update when a resize moves non-client strips.

Native non-client peers must preserve the same in-host edge reservation; they
must not ask a top-level layout manager to reserve a second strip. Windows
maps `status_bar` parts to the common-controls `STATUSCLASSNAME` child with
`SB_SETPARTS` and `SB_SETTEXT`, while portable code retains part text, order,
widths, visibility, and reserved geometry. XView frame footers and Motif's
`XmNmessageWindow` remain unsuitable for this contract because both alter the
outer content geometry. A backend with no exact peer keeps the themed painter.

`directory_dialog` shares `file_dialog`'s logical modal lifecycle and accepted
path storage. It must invoke the platform's standard folder chooser or folder
mode. `open_file_dialog` and `save_file_dialog` remain the standard file APIs.
Unsupported optional multiple selection may reduce to one accepted path, but
the backend must not substitute an application-painted chooser when its
platform supplies a standard one.

SDL2 consistently uses Native's themed, keyboard-aware C++ filesystem browser
for file open, file save, and folder selection. The shared shell provides a
compact Places table populated from detected special directories, real PNG
file/folder icons, icon-only back/forward/up history, and a clickable
horizontal breadcrumb. Double-clicking the breadcrumb toggles direct location
editing in the same bounds; double-clicking the editor restores the
breadcrumb without discarding uncommitted text, and Ctrl+L also enters direct
editing. Entries occupy a multi-column
Name/Type/Size table with a continuous, draggable classic scrollbar and no
pagination; file modes add a separate filename field. A single click selects;
a double click enters a folder or accepts an existing file. The primary action
validates the mode-specific selection. The browser applies open filters,
appends a save extension before overwrite validation, and restores its owner
after either result.

`message_box::show()` is synchronous and owner-modal. It maps the requested
one-, two-, or three-button set and semantic icon to the platform alert and
returns a typed `message_box_result`. Closing the native alert maps to
`cancel` when the button set includes Cancel and otherwise to `none`.
SDL2 implements that alert as a library-owned modal window so its control font,
panel background, semantic icon, and real `button` controls match the rest of
the SDL window. SDL2 and Window Maker use the same attributed, embedded PNG
badge set for information, warning, error, and question requests; no font glyph
or procedural approximation participates in those symbols. SDL's nested wait
continues painting and dispatching dialog input and restores owner focus before
returning.

A `non_client` object is application-owned and attaches non-owningly to one
`wnd`. Every visible strip reserves a non-negative extent at one window edge
of its host's chrome area, not of the raw window bounds, so a host that owns
its own edge furniture keeps its strips inside that furniture.
`wnd::get_client_bounds()` returns the remaining host-relative rectangle, and
layout managers must receive that rectangle. Visibility, extent, attachment,
detachment, and window resize must trigger layout and paint updates without
moving the top-level native frame.

Rulers may occupy all four edges. Orientation follows the edge. Origins,
units-per-pixel, and positive minor/major intervals are finite doubles.
Optional pointer tracking uses host-relative motion, updates the cached value,
and emits once per changed ruler pixel. Status bars occupy the full bottom edge,
including both lower corners, while side non-client strips stop above them.
Status bars divide their width into fixed parts and equal shares of remaining
flexible parts. Painted status strips and their parts use the panel/chrome
surface rather than the white editable-content surface; part edges remain
visually distinct through the active theme's highlight and shadow roles.

The complete default rendering must occur inside each protected virtual draw
stage. Derived classes may replace a stage or call its base implementation and
augment it; no owner-draw pass may repaint the default afterward. Rulers expose
background, tick, label, and tracker stages. Status bars expose background and
part stages. The painting uses the graphics context of the active paint event
and the current backend theme.

Vision exposes the combined demonstration through **Window -> Input and
window chrome**. Portable tests cover pre-create state, silent programmatic
updates, native event ordering, edge reservation, relayout, and ruler
tracking; Docker builds compile every backend implementation.

## 18. Structural containers

`panel` is a concrete, empty child `wnd` whose purpose is to parent and lay out
other windows. Its header is `include/native/panel.h` and is exported through
`include/native.h`. It adds no collection API: child registration, ownership,
geometry, and layout are the inherited `wnd` facilities. Children call
`set_parent()`, the panel owns a `layout_manager` through `set_layout()`, and
`get_client_bounds()` remains the layout region after non-client edges.

A panel is a child container, never a top-level window. The caller owns the
panel and every child; the panel only borrows children through the existing
parent relationship. Destroying a panel destroys the native resources of
created children without deleting their C++ objects. Reparenting follows
`wnd::set_parent()` validation, including cycle rejection.

`panel::create()` creates only the panel's own backend host and is idempotent.
It must establish the backend parent binding and set `get_created()` before
emitting `on_wnd_create`, so that handler can create children. `show()` shows
only the panel; `destroy()` is idempotent and releases created child resources
through the inherited destruction path. A panel never creates or shows a
registered child implicitly, because it cannot know whether that child should
start visible.

A panel is visually empty: no caption, border, bevel, focus ring, or padding,
and it is not keyboard-focusable merely by existing. Exposed panel space shows
the backend's ordinary themed container background and must not retain stale
pixels. Visible children receive hit-tested pointer input before the panel, an
event consumed by a child is not delivered again as a panel event, and empty
panel space activates nothing. Inherited pointer signals may report otherwise
unconsumed empty-space events; the panel adds no semantic action signal.

A panel and a `canvas` are distinct, unrelated siblings below `wnd`.
Inheritance must not be used to merge their roles: a panel hosts controls and a
canvas draws application content.

## 19. Paintable child surfaces

`canvas` is a concrete child `wnd` whose entire client viewport is owned by
application painting, together with optional horizontal and vertical
scrollbars. Its header is `include/native/canvas.h`. It uses the signals
already defined by `wnd` — `on_wnd_paint`, `on_mouse_move`, `on_mouse_click`,
`on_mouse_wheel`, and the lifecycle and geometry signals — and adds only
`on_scroll`. A canvas is not a general child-control host.

`scrollbar_policy` is the shared control concept declared in
`include/native/scrollbar.h`; `canvas` and `table_view` use that one
definition.

Content bounds are half-open and expressed in signed 32-bit content pixels
through `canvas_content_bounds`, independent of `coord` and `dim`. A negative
origin is valid. The scroll position identifies the content coordinate mapped
to the viewport's leading edge. Given content interval `[origin, origin + span)`
and viewport span `page`, the valid interval is
`[origin, max(origin, origin + span - page)]`; empty content uses position
zero. Bounds arithmetic must be checked and must not wrap.

The default policy on each axis is `automatic`, which shows a scrollbar only
when the content cannot fit the available viewport. `always` reserves the
themed extent and draws an inactive scrollbar disabled. `never` neither
reserves nor draws one, and does not disable programmatic scrolling. Because
one scrollbar shrinks the other axis, automatic visibility must be evaluated
until neither decision changes.

Scrollbars reserve the outermost canvas edges. `wnd::get_chrome_bounds()` is
the protected hook a control overrides to declare such reservations; non-client
strips stack inside the returned area, so a ruler stops at a visible scrollbar
rather than running under it. `get_client_bounds()` is therefore the drawable
viewport after both scrollbars and rulers, and scrollbar tracks are never
included in application paint bounds.

`set_scroll_position()` clamps, invalidates on a change, and emits nothing.
A backend scrollbar action calls `on_native_scroll()` with an absolute content
position; that function clamps, stores, invalidates, and emits `on_scroll`
exactly once when the effective position changed. Wheel input over a movable
axis scrolls it, and the normalized `on_mouse_wheel` event is still emitted
exactly once. Thumb size and position represent the viewport inside the full
32-bit range and must let both endpoints be reached exactly. Scrollbar
appearance, hit geometry, minimum thumb size, and extents come from theme
metrics and `theme::draw_scrollbar_part`.

One backend paint notification dispatches exactly one logical
`wnd_paint_event`. The supplied drawer is clipped to the viewport and must not
reach a sibling, the parent, a ruler, or a scrollbar track. The canvas adds no
border, label, focus ring, padding, or theme fill, and must not clear over
application output after the callback. Scrollbars and their corner filler are
control chrome, painted after non-client strips and never delegated to the
paint subscriber. Native does not translate the drawer by the scroll position;
paint rectangles and pointer positions stay viewport-local, and the subscriber
applies its own content transform.

Pointer actions inside scrollbar tracks, thumbs, and step controls are handled
as chrome and are not emitted as client mouse clicks. Every other pointer event
reaches the inherited `wnd` dispatch with canvas-local coordinates.
