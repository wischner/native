# Chapter 17: Input, Dialogs, and Window Chrome

This chapter combines the standard choice controls, operating-system dialogs,
and edge-attached window elements used by a typical document window.

When custom-painting modal labels, use the theme's
`get_button_foreground_color()` so light and dark native dialog palettes
remain readable. Rulers use `surface_kind::ruler` for their paper; the
library paints shared perpendicular corners with `ruler_corner`. Motif's
menu-colored ruler paper and marks do not change ordinary headers or
window backgrounds.

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

Selection-only combos open from either the field or their downward arrow. In
an editable combo, clicking the text area focuses editing while the arrow
opens the list. The popup chooses the available side of the control and stays
above normal sibling content for both painting and pointer input. The arrow is
a compact filled mark on a white content button, the complete border remains
visible, and both the selected value and list rows use the ordinary control
font after every selection.

On SDL2, clicking either part of an editable combo opens it while preserving
keyboard editing. Moving through the open popup highlights the row under the
pointer without committing it until clicked.

## File icons and special directories

Filesystem-facing APIs use `std::filesystem::path`. Refresh the system
directory snapshot, find a semantic location, and request its icon without
including a platform header:

```cpp
native::special_directory::detect();
const native::special_directory *documents =
    native::special_directory::find(
        native::special_directory_kind::documents);

if (documents) {
    const std::filesystem::path path = documents->get_path();
    const native::file_icon icon = documents->get_icon(32);
    const native::img image = native::img::decode(
        icon.get_png().data(), icon.get_png().size());
}
```

`file_icon::from_path(path, size)` detects whether an existing entry is a file
or directory. Use `for_file()` or `for_directory()` when the path may not yet
exist. `get_png()` always returns a complete PNG with exactly the requested
width and height. `get_source()` and `is_generic()` report whether the image
came from the operating system or the portable fallback.

The snapshot also supports `count()`, `at(index)`, and `find(kind)`. Its paths
are discovered, not created, and a later `detect()` invalidates earlier
pointers into the snapshot.

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
            status.set_text(choose_folder.get_path().string());
        return true;
    });

choose_folder.create();
choose_folder.show();
```

Keep the object alive until completion. Use `open_file_dialog` for existing
files and `save_file_dialog` for a destination filename, including filters,
suggested names, default extensions, and native overwrite confirmation.
On SDL2, the same API consistently opens Native's themed C++ file/folder
browser, so the three modes have one design on every desktop. The browser
uses detected special folders in a compact Places table, native-or-generic PNG
icons, icon-only history navigation, and a clickable horizontal breadcrumb.
Double-clicking that area toggles between breadcrumb and direct path editing;
the editor keeps uncommitted text across those view changes, and Ctrl+L also
enters direct editing. The
Name/Type/Size file table uses one continuous draggable scrollbar rather than
pagination. Open and save modes keep the filename separate from the current
location.

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
SDL's alert uses the same font, gray panel, semantic icon, and button drawing
as its other windows. Its attributed embedded PNG badges use consistent
colored silhouettes and white marks. Window Maker places the same semantic
badge in its native WINGs alert while retaining the WINGs fonts and buttons,
with the requested title on the native frame. The owner is ready for a normal
first click as soon as the call returns, and a click that first activates an
SDL window is delivered to the control under it.

## Rulers and status bar

For a status strip that matches the current backend's chrome, set its extent
after window creation to `theme::create(get_gpx())->get_status_bar_height()`.
Vision's Input and window chrome gallery uses this rule. GEMix matches the
AES title/menu height, including clearance between text and the top separator.

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
                  .set_edge_visible(true)
                  .set_track_mouse(true);
        vertical.set_minor_tick(10)
                .set_major_tick(50)
                .set_edge_visible(true)
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

Ruler edge rules are hidden by default. `set_edge_visible(true)` draws a
continuous bottom line on a horizontal ruler or right line on a vertical
ruler. The rule uses the same theme color as the ticks; its state is returned
by `get_edge_visible()`.

Status parts with a positive width are fixed. Parts whose width is zero share
the remaining space. A bottom status bar spans the full window width and owns
both lower corners; side rulers and other side strips stop above it. Both
controls provide protected virtual drawing stages, so an application-specific
derived control can call the base implementation and add detail without a
duplicate default-paint pass.

Painted status bars use the platform's panel/chrome background and subtle
part edges. White content backgrounds remain available for text editors,
lists, icon views, trees, and tables.

Windows uses the native common-controls status bar for the exact base
`status_bar` type and maps these widths and strings with `SB_SETPARTS` and
`SB_SETTEXT`. A derived status bar retains the custom drawing path so its
protected drawing-stage overrides remain effective.

On Haiku, the compact status strip follows the system `StatusView` chrome:
the menu-bar background, a dark top rule, short cell dividers, panel text, and
a platform theme height aligned with the titled-window resize marker. This
height is distinct from the smaller scrollbar extent. Haiku's similarly named
`BStatusBar` is a progress indicator, so it is not used for this text-only,
multipart strip.

Return to the [manual contents](../PROGRAMMING-NATIVE.md).

Dense ruler scales retain every configured minor and major tick, while
labels are spaced using measured text width to avoid overlap on either
axis. Painting is clipped to the ruler strip and restores the caller’s
graphics state. Choose a strip extent that leaves room for the font above
the ticks (Maestro uses 32 pixels for one-pixel minor/eight-pixel major ticks).

## Properties and tools

A compact inspector uses standard editors for each value kind. Create it
only after its parent, as with other controls:

```cpp
native::property_grid properties(12, 40, 300, 220);
properties.set_parent(&window);
properties.set_items({
    {"name", "Name", native::property_kind::text, std::string("Shape"), {}, false},
    {"width", "Width", native::property_kind::number, 120.0, {}, false},
    {"visible", "Visible", native::property_kind::boolean, true, {}, false},
    {"mode", "Mode", native::property_kind::choice, std::string("Fill"),
        {"Fill", "Outline"}, false}
});
properties.on_change.connect([](native::property_change change) {
    // change.id identifies the row; change.value has its declared type.
    return true;
});
properties.create();
properties.show();
```

Rows default to the control font height plus six pixels. `set_row_height(0)`
restores that compact default, and `set_label_width()` sets the first column's
width. Labels and value fields share the control background. The scrollbar
sits at the right edge and only complete rows expose editors. `set_value()`
updates silently. Read-only properties still accept
programmatic updates. Invalid values and duplicate IDs throw; user changes
are emitted only after validation.

Declare bars after the window they borrow, and keep them alive while displayed:

```cpp
native::toolbar commands(window, native::window_edge::top);
native::toolbar drawing(window, native::window_edge::left);
commands.add_item({"save", "Save", native::tool_kind::button, {}, false, true, {}});
drawing.set_items({
    {"select", "Select", native::tool_kind::exclusive, "tools", true, true, {}},
    {"draw", "Draw", native::tool_kind::exclusive, "tools", false, true, {}}
});
drawing.set_extent(80); // room for vertical tool labels
commands.on_command.connect([](native::tool_command command) {
    // Execute command.id; command.checked is the resulting sticky state.
    return true;
});
```

Use `tool_kind::toggle` for independent sticky buttons. Exclusive groups are
scoped to their bar. Add more toolbar objects to stack multiple bars at an
edge; there is no fixed bar-count limit. Use `set_edge()` to relocate one.
Toolbars reserve space through `window.get_client_bounds()` and do not enter
its ordinary child layout. Place client controls inside those bounds.

The last `tool_item` field is a `std::shared_ptr<const native::img>`. Assign an
image and use `set_icon_size({16, 16})`, `{24, 24}`, or `{32, 32}`. Images can be
used without a text label. Use transparent image pixels around the artwork
to preserve the toolbar background. Images and labels are centered together.
The default icon size is 16 by 16 pixels; any supported positive dimensions
can be passed to `set_icon_size(native::size(24, 24))`. The toolbar scales the
source image to that size. `native::img` loads PNG or JPEG data; PNG supports
the transparent background commonly used for toolbar artwork.
The bar grows or shrinks with the selected image size and retains its padding;
neighbors and the client area are updated in the same pass. Top and bottom
bars span the window width, and side bars stop between them;
its tools use the main-menu palette and selection style. Available strip
length clips excess tools, so use another bar to expose additional tools.

```cpp
properties.set_border_sides(native::border_sides::left |
                             native::border_sides::bottom);
commands.set_border_sides(native::border_sides::bottom);
```

Every window/control and non-client strip starts with `border_sides::all`.
The mask hides existing outer edges; it does not create a border on a bare
panel, hide checkbox indicators, or change window-manager decorations.

A custom dropdown accepts any C++ value and any control, including a canvas
or a derived panel that owns several child controls:

```cpp
auto editor = std::make_shared<native::property_drop_down>();
editor->content_size = {160, 32};
editor->to_text = [](const native::property_value &value) {
    return std::to_string(std::any_cast<int>(std::get<std::any>(value))) + " px";
};
editor->create_content = [](const native::property_value &,
                            native::property_drop_down::commit commit) {
    auto choice = std::make_unique<native::button>("Use 24 px");
    choice->on_click.connect([commit] {
        commit(std::any(24));
        return true;
    });
    return choice;
};
properties.add_item({"custom", "Custom size", native::property_kind::drop_down,
    std::any(16), {}, false, editor});
```

Return an uncreated control. The grid takes ownership and handles its parent,
bounds, creation, and display. A derived container can create its own children
in `on_wnd_create`. Call the supplied callback to commit and close; Cancel
closes without changing the property. The converter paints the closed value.
Programmatic changes remain silent. Each accepted `std::any` commit emits a
change, since arbitrary values have no generic equality comparison. Removing
the source row cancels its popup and invalidates retained callbacks.

Vision's **Custom canvas** property demonstrates the same API with a color
palette painted on a canvas and an `rgba`-to-hex converter.
