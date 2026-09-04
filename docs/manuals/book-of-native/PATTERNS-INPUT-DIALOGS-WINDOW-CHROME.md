# Patterns: Input, Standard Dialogs, and Window Chrome

This chapter describes the portable contracts for combo boxes, list boxes,
standard directory and message dialogs, and edge-attached rulers and status
bars. Public objects keep their state in C++; each backend adapts that state to
its native widget or to its existing native-theme renderer.

## Combo boxes and list boxes

`combo_box` supports `drop_down_list` and `editable` styles. Its item model,
selected index, and displayed text remain available before creation and after
native resources are destroyed. An index of `-1` means no list item is
selected. Selection-only text must identify an existing item; editable text
may contain any UTF-8 value.

Programmatic setters are silent. Native user actions first update the cache,
then enter the protected-compatible virtual event hooks
`on_native_selection()`, `on_native_text()`, and `on_native_drop_down()`.
Their base implementations emit `on_selection_change`, `on_text_change`, and
`on_drop_down`. Derived controls can add behavior and call the base hook when
the normal signal is still wanted.

Backends use their standard combo widget where one exists: Win32 `COMBOBOX`,
AppKit `NSComboBox`, Motif `XmComboBox`, Haiku `BOptionPopUp`, WINGs popup and
text controls, and XView choice and text controls. Athena composes its standard
text, menu-button, and menu widgets. SDL2 and GEMix route the control through
their existing backend-owned theme and input systems.
In Window Maker editable mode, the arrow-only native popup button is inset at
the right edge of the full-width native text field. It does not consume a
second control-sized region or extend beyond the field. Its WINGs menu remains
the width of the complete combo; the backend shapes only the visible and
clickable closed-button region down to the arrow and centers the disclosure
glyph within that region. Press and release repaint that same centered geometry
on an input-transparent overlay, so the native popup's synchronous selection
animation cannot reveal or leave its own off-center indicator behind.

The emulated interaction follows desktop combo conventions. The indicator is
a compact filled downward arrow on a content-colored button which remains
inset within the continuous outer frame. A selection-only field toggles from
the whole field; an editable field focuses text from its content and toggles
from the arrow.
Popups prefer the space below, fall back above, paint after ordinary sibling
controls, receive pointer input before any sibling they cover, retain a
complete one-pixel frame above their rows, and consume the outside click that
dismisses them. Both the closed value and popup rows select the stock control
font explicitly, so a prior sibling paint or a selection change cannot alter
their text size.

SDL2's fully painted editable field combines those actions: either the text
area or arrow toggles the popup, and the text area retains keyboard input.
Pointer motion through the popup moves one hot-row highlight without changing
the committed selection.

`list_box` is a descriptive alias of `list`, so existing list code and derived
classes remain source compatible:

```cpp
native::combo_box format(
    {"Plain text", "Markdown", "HTML"},
    native::combo_box_style::drop_down_list,
    20, 20, 180, 26);
native::list_box files({"README.md", "LICENSE"}, 20, 60, 240, 140);
```

The fallback `draw_control()` virtual paints the complete combo box. As with
the other controls, overriding it replaces that stage; the library does not
paint a second default pass afterward.

## Filesystem resources

`file_icon` turns a `std::filesystem::path` into an exact-size square PNG.
`from_path()` uses `std::filesystem::is_directory()` to select the entry type.
For a destination that does not exist yet, `for_file()` and
`for_directory()` state the intended type explicitly. The returned
`file_icon_source` distinguishes an operating-system image from the shared
attributed generic file and folder PNG fallbacks.

Linux resolves PNGs through the current Freedesktop icon theme and its
inherited, Adwaita, and hicolor fallbacks. Windows renders the Shell icon,
Haiku renders the Tracker icon, and macOS renders the `NSWorkspace` file icon.
Each adapter transfers only RGBA pixels into shared code; the public class
contains no native handle.

`special_directory::detect()` refreshes the process snapshot of conventional
locations. Its values use `std::filesystem::path` and cover home, desktop,
documents, downloads, music, pictures, videos, public and template folders,
applications, fonts, configuration, application data, cache, and temporary
storage when the platform supplies them. Linux reads the XDG base and user
directory configuration through standard streams, Windows uses Known Folders,
Haiku uses `find_directory`, and macOS uses Foundation search paths. Detection
does not create or require the returned directories. A second `detect()` may
invalidate pointers returned by `at()` or `find()`.

## Standard dialogs

`directory_dialog` extends the existing `file_dialog` state and modal-session
contract. It accepts an initial folder and optionally requests multiple
folders, then reports `accepted` or `cancelled` through `on_modal_close`.
`get_path()` returns the first accepted folder and `get_paths()` returns all
folders supplied by a capable platform chooser.

```cpp
native::directory_dialog folder(window, "Choose workspace");
folder.set_initial_path("/projects");
folder.on_modal_close.connect([&](native::dialog_result result) {
    if (result == native::dialog_result::accepted)
        open_workspace(folder.get_path());
    return true;
});
folder.create();
folder.show();
```

The existing `open_file_dialog` and `save_file_dialog` remain the standard
file-open and file-save APIs. All three chooser objects must outlive an active
native session because some platforms complete asynchronously.

SDL2 consistently uses one library-owned themed browser for file open, file
save, and folder selection. Its path handling, traversal, metadata, and
wildcard matching use the C++ standard library. The window has a Places
table populated from `special_directory`, icon-only back/forward/up
navigation, and a clickable, horizontally eliding breadcrumb. The address
editor occupies the same bounds: a double click toggles between breadcrumb
and direct path modes, while Ctrl+L also opens direct editing. Entering direct
mode places a normal caret without implicitly selecting the complete path, and
switching views preserves uncommitted path text. The main
Name/Type/Size `table_view` and Places rows use decoded
`file_icon` PNGs; file icon lookup is cached by type so large folders remain
responsive. Its classic scrollbar moves through one continuous directory
listing and supports arrows, trough steps, wheel input, and thumb dragging;
the browser never paginates the directory. A single click selects an entry, a
double click enters a folder or accepts a file, and Enter works in the focused
location, filename, or table. File modes keep location and filename in
separate fields; Ctrl+H toggles dot-prefixed hidden entries. The browser also
provides filtering, validation, default save extensions, and overwrite
confirmation. Completion releases capture and returns keyboard focus to the
owner.

`message_box::show()` is synchronous and owner-modal. The button sets are
`ok`, `ok_cancel`, `yes_no`, and `yes_no_cancel`; the result is one of `ok`,
`cancel`, `yes`, `no`, or `none`. Information, warning, error, and question
icons are semantic requests that the backend maps to its standard alert.
SDL2 uses a library-owned modal alert with the same stock control font, panel
surface, semantic icon, and `button` implementation as the rest of its UI. Its
information, warning, error, and question badges come from an attributed,
embedded PNG set, so they do not depend on SDL_ttf glyph coverage or
procedurally approximated marks. Window Maker places the same attributed
semantic badge in its native WINGs alert, preserves the alert's fonts, layout,
and buttons, and applies the requested title to its native frame. Its private
modal dispatcher continues owner expose handling and deferred Native
callbacks. SDL's synchronous wait
continues to process paint, pointer, keyboard, resize, and close events before
restoring the owner.

```cpp
const auto answer = native::message_box::show(
    window, "Save changes before closing?", "Editor",
    native::message_box_buttons::yes_no_cancel,
    native::message_box_icon::question);
```

## Non-client edge strips

`non_client` is the common base for window-owned elements that consume an
edge instead of participating in child layout. Visible elements reserve their
extent from `wnd::get_client_bounds()`. Layout managers receive that reduced,
host-relative rectangle. Hiding a strip or changing its extent immediately
relayouts children and invalidates the host.

Strips stack inside the host's chrome area rather than its raw bounds, so a
host that owns permanent edge furniture of its own keeps them inside it. On a
`canvas`, whose scrollbars take the outermost edges, a horizontal ruler
therefore stops before a visible vertical scrollbar instead of running under
it. See [Structural Panels and Paintable Canvases](PATTERNS-PANELS-AND-CANVASES.md).

The strips are ordinary C++ members owned by the application. They attach in
their constructor and detach in their destructor; the host never deletes
them. They paint through the graphics context supplied by the current window
paint event.

## Rulers

A `ruler` may occupy any edge. Top and bottom rulers are horizontal; left and
right rulers are vertical. `set_origin()` defines the value at the strip's
first pixel and `set_units_per_pixel()` defines the scale. Minor and major
tick intervals are expressed in those same units.

```cpp
native::ruler horizontal(window, native::window_edge::top, 24);
native::ruler vertical(window, native::window_edge::left, 30);

horizontal.set_origin(-20)
          .set_units_per_pixel(0.5)
          .set_minor_tick(5)
          .set_major_tick(25)
          .set_edge_visible(true)
          .set_track_mouse(true);
horizontal.on_tracking.connect([](double value) {
    update_coordinate_readout(value);
    return true;
});
```

Tracking observes host pointer motion and emits only when the tracked ruler
pixel changes. The protected virtual stages `draw_background()`,
`draw_tick()`, `draw_label()`, `draw_edge()`, and `draw_tracker()` perform the
actual default drawing. The optional edge is the bottom rule for horizontal
rulers and the right rule for vertical rulers, using the tick color. A derived
ruler can replace one stage and call its base implementation when it wants the
standard appearance plus an addition.

## Status bars

`status_bar` reserves the bottom edge. `set_text()` displays one flexible
part. `set_parts()` accepts ordered `status_bar_part` values; a positive width
is fixed and width zero shares the remaining width among all flexible parts.
The status bar spans the full window width and owns both lower corners; side
non-client strips stop above it.

```cpp
native::status_bar status(window, 22);
status.set_parts({{"Ready", 0}, {"Line 12, Column 4", 150}});
```

`draw_background()` and `draw_part()` are protected virtual stages and contain
the default themed painting. Status bars and rulers use semantic theme
surfaces and the active backend palette, rather than hard-coded visuals from
another desktop. Painted status parts use the gray panel/chrome role with
theme highlight and shadow edges, leaving white content surfaces for editors
and item views.

Windows hosts `STATUSCLASSNAME` for the exact base `status_bar`, maps portable
parts through `SB_SETPARTS` and `SB_SETTEXT`, and lets the common control own
the size grip and painting. Derived status bars stay on the painted path so
their protected-stage overrides continue to work.

The Haiku backend uses its native compact status-view recipe: a system
menu-bar background, dark top rule, short dividers, panel text, and a platform
theme height that aligns with the titled-window resize marker. That height is
distinct from the smaller scrollbar extent. `BStatusBar` is Haiku's progress
indicator; it does not represent this text-only multipart window strip.

Vision exposes these APIs under **Window -> Input and window chrome**.
For automated desktop smoke tests, `vision --input-chrome` opens the same
window immediately after the application enters its normal event loop.
