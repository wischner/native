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

`message_box::show()` is synchronous and owner-modal. The button sets are
`ok`, `ok_cancel`, `yes_no`, and `yes_no_cancel`; the result is one of `ok`,
`cancel`, `yes`, `no`, or `none`. Information, warning, error, and question
icons are semantic requests that the backend maps to its standard alert.

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
          .set_track_mouse(true);
horizontal.on_tracking.connect([](double value) {
    update_coordinate_readout(value);
    return true;
});
```

Tracking observes host pointer motion and emits only when the tracked ruler
pixel changes. The protected virtual stages `draw_background()`,
`draw_tick()`, `draw_label()`, and `draw_tracker()` perform the actual default
drawing. A derived ruler can replace one stage and call its base implementation
when it wants the standard appearance plus an addition.

## Status bars

`status_bar` reserves the bottom edge. `set_text()` displays one flexible
part. `set_parts()` accepts ordered `status_bar_part` values; a positive width
is fixed and width zero shares the remaining width among all flexible parts.

```cpp
native::status_bar status(window, 22);
status.set_parts({{"Ready", 0}, {"Line 12, Column 4", 150}});
```

`draw_background()` and `draw_part()` are protected virtual stages and contain
the default themed painting. Status bars and rulers use semantic theme
surfaces and the active backend palette, rather than hard-coded visuals from
another desktop.

Vision exposes these APIs under **Window -> Input and window chrome**.
For automated desktop smoke tests, `vision --input-chrome` opens the same
window immediately after the application enters its normal event loop.
