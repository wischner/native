# Chapter 17: Input, Dialogs, and Window Chrome

This chapter combines the standard choice controls, operating-system dialogs,
and edge-attached window elements used by a typical document window.

## Combo box and list box

A selection-only combo accepts text from its item model. An editable combo
also accepts arbitrary UTF-8 input:

```cpp
native::combo_box mode(
    {"Automatic", "Compact", "Detailed"},
    native::combo_box_style::drop_down_list,
    40, 50, 220, 26);

native::combo_box scale(
    {"25%", "50%", "100%", "200%"},
    native::combo_box_style::editable,
    40, 90, 220, 26);

mode.set_selected_index(0);
scale.set_text("125%");
```

Connect `on_selection_change` for item selection and `on_text_change` for the
complete displayed value. Setters update the control without emitting these
user-action signals. `list_box` is an alias of the existing `list` control and
uses the same item and selection API.

## Tabs and borrowed pages

`tab_view` owns its ordered `tab_item` model and borrows one uncreated `wnd`
for each page. Declare pages before the tab view, add them before creating the
control, and create only the tab view itself:

```cpp
native::list general({"Identity", "Appearance"});
native::list advanced({"Caching", "Diagnostics"});
native::tab_view tabs(20, 60, 360, 220);

tabs.add_item("General", general);
tabs.add_item("Advanced", advanced);
tabs.on_selection_change.connect([](int index) {
    return true; // A user selected a different page.
});

tabs.set_parent(&window);
tabs.create();
tabs.show();
```

Only the selected page has a native resource. Changing selection destroys the
old page resource and creates the new one while preserving both portable page
objects. `set_selected_index()` is silent. Individual items expose a mutable
UTF-8 title and enabled state. Haiku uses `BTabView`/`BTab`, Windows uses the
common tab control, macOS uses `NSTabView`/`NSTabViewItem`, and backends without
a standard tab peer use the same themed portable renderer and lifecycle.
`set_tab_placement()` selects top, bottom, left, or right; side labels are
rotated and all placement changes preserve the selected borrowed page.

## Directory and message dialogs

Directory selection follows the same reusable logical-dialog lifecycle as
file open and file save:

```cpp
native::directory_dialog choose_folder(window, "Choose output folder");

choose_folder.on_modal_close.connect(
    [&](native::dialog_result result) {
        if (result == native::dialog_result::accepted)
            status.set_text(choose_folder.get_path());
        return true;
    });

choose_folder.create();
choose_folder.show();
```

Keep the object alive until completion. Use `open_file_dialog` for existing
files and `save_file_dialog` for a destination filename, including filters,
suggested names, default extensions, and native overwrite confirmation.

For a short blocking question, use the standard message box:

```cpp
const native::message_box_result result = native::message_box::show(
    window,
    "Replace the existing document?",
    "Confirm Save",
    native::message_box_buttons::yes_no_cancel,
    native::message_box_icon::warning);
```

Button sets contain one, two, or three buttons and return a typed result. The
dialog remains owner-modal and uses the active platform's alert presentation.

## Rulers and status bar

Rulers and status bars are non-client objects. Constructing one attaches it to
its window edge and removes that extent from the rectangle passed to the
window's layout manager:

```cpp
class document_window : public native::app_wnd
{
public:
    document_window()
        : native::app_wnd("Document", 80, 80, 800, 600),
          horizontal(*this, native::window_edge::top, 24),
          vertical(*this, native::window_edge::left, 30),
          status(*this, 22) {
        horizontal.set_minor_tick(10)
                  .set_major_tick(50)
                  .set_track_mouse(true);
        vertical.set_minor_tick(10)
                .set_major_tick(50)
                .set_track_mouse(true);
        status.set_parts({{"Ready", 0}, {"100%", 80}});
    }

private:
    native::ruler horizontal;
    native::ruler vertical;
    native::status_bar status;
};
```

Use `get_client_bounds()` when positioning client content manually. A layout
manager receives this reduced rectangle automatically. Ruler origin, scale,
minor ticks, and major ticks use `double`, while the strip extent remains a
pixel count. Mouse tracking exposes the current coordinate through
`get_tracked_value()` and `on_tracking`.

Status parts with a positive width are fixed. Parts whose width is zero share
the remaining space. A bottom status bar spans the full window width and owns
both lower corners; side rulers and other side strips stop above it. Both
controls provide protected virtual drawing stages, so an application-specific
derived control can call the base implementation and add detail without a
duplicate default-paint pass.

Windows uses the native common-controls status bar for the exact base
`status_bar` type and maps these widths and strings with `SB_SETPARTS` and
`SB_SETTEXT`. A derived status bar retains the custom drawing path so its
protected drawing-stage overrides remain effective.

On Haiku, the compact status strip follows the system `StatusView` chrome:
the menu-bar background, a dark top rule, short cell dividers, panel text, and
the horizontal-scrollbar height. Haiku's similarly named `BStatusBar` is a
progress indicator, so it is not used for this text-only, multipart strip.

Return to the [manual contents](../PROGRAMMING-NATIVE.md).
