# Chapter 22: Toolbars and Property Inspectors

Use `toolbar` for edge-attached commands and `property_grid` for a small typed
inspector. The grid composes the library's standard editors. Toolbars and
status bars attach to a window automatically; they are not child controls
that you create manually.

Property grid rules end with the last visible property. Increasing the grid's
height leaves an empty area below the rows, without extra value-side lines.

## Step 1: Describe commands and values

A tool has a string ID, text, kind, exclusive group, checked state, enabled
state, and optional shared image. A property has a string ID, label, kind,
typed value, choices, and read-only flag. Use IDs in handlers rather than
matching translated labels.

## Step 2: Attach chrome and create the grid

A `tool_kind::control` item hosts any control, such as a font `combo_box`, with
`control` and `length` set. The toolbar parents, creates and places it. Declare
the control before the toolbar. A bar with hosted controls uses Native's portable
painting on every backend; a bar of plain tools keeps its OS peer.

Declare chrome after its owner. Constructing a toolbar or status bar reserves
its strip. Create ordinary client content in `on_wnd_create` and size it from
`get_client_bounds()` so it clears the strips. A status bar occupies the bottom
below bottom toolbars. The following complete program uses a fixed-width grid
for readability; a larger application can install a layout manager.

## Step 3: Connect user actions

```cpp
//
// Connects edge commands and a typed inspector to one status display.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include <any>
#include <memory>
#include <string>
#include <variant>

class inspector_window : public native::app_wnd {
public:
    // Chrome borrows this window and is destroyed before its owner.
    inspector_window()
        : app_wnd("Inspector", 100, 100, 660, 420),
          _commands(*this, native::window_edge::top), _status(*this, 24) {
        _commands << native::tool_item{"reset", "Reset",
                         native::tool_kind::button, {}, false, true, {}}
                  << native::tool_item{"", "",
                         native::tool_kind::separator, {}, false, true, {}}
                  << native::tool_item{
                         "snap", "Snap", native::tool_kind::toggle,
                         {}, false, true, {}};
        _properties.set_items({
            {"name", "Name", native::property_kind::text,
             std::string("Rectangle"), {}, false},
            {"width", "Width", native::property_kind::number, 120.0, {}, false},
            {"visible", "Visible", native::property_kind::boolean, true, {}, false},
            {"mode", "Mode", native::property_kind::choice,
             std::string("Fill"), {"Fill", "Outline"}},
            {"id", "ID", native::property_kind::text,
             std::string("shape-1"), {}, true}
        });
        _status.set_parts({{"Ready", 0}});
        _properties.on_change.connect([this](native::property_change change) {
            _status.set_parts({{"Changed: " + change.id, 0}});
            if (change.id == "name")
                set_title("Inspector: " + std::get<std::string>(change.value));
            return true;
        });
        _commands.on_command.connect([this](native::tool_command command) {
            if (command.id == "reset") {
                _properties.set_value("name", std::string("Rectangle"));
                _properties.set_value("width", 120.0);
                _commands.set_checked("snap", false);
                set_title("Inspector");
            }
            _status.set_parts({{command.id == "snap"
                ? (command.checked ? "Snap on" : "Snap off")
                : "Reset values", 0}});
            return true;
        });
        on_wnd_create.connect([this] {
            _properties.set_parent(this);
            _properties.set_bounds(get_client_bounds());
            _properties.create();
            _properties.show();
            return true;
        });
        on_wnd_resize.connect([this](native::size) {
            _properties.set_bounds(get_client_bounds());
            return true;
        });
    }

private:
    native::property_grid _properties;
    native::toolbar _commands;
    native::status_bar _status;
};

// Run the inspector using the selected platform's controls.
int program(int, char **) {
    inspector_window window;
    return native::app::run(window);
}
```

Edit Name and Width, select Outline, toggle Snap, and press Reset. The status
reports user actions. Reset uses silent setters, so it explicitly updates the
title and status rather than expecting user-change signals.

## Add images and exclusive tools

A `tool_item::icon` is a `std::shared_ptr<const img>`. Load a PNG once and share
it between tools. Transparent image pixels retain the toolbar background.
`set_icon_size({24, 24})` changes the image box and the toolbar's thickness.
A tool with an icon and no text is square, as long as the strip is thick;
`get_item_bounds(id)` returns where a tool sits on the strip.
Use `tool_kind::exclusive` and a common nonempty group string for mutually
exclusive tools on one bar. A separator has no command and may use an empty ID.

Bars can attach to any edge. `set_edge()` moves one; multiple bars can share an
edge. Use adequate `set_extent()` for vertical labels. Available strip length
clips overflowing tools, so allocate space for the commands you expose.

## Add a custom property dropdown

Inside the window constructor, before grid creation, add this self-contained
popup descriptor. It returns an uncreated child that the grid owns:

```cpp
auto preset = std::make_shared<native::property_drop_down>();
preset->content_size = {180, 40};
preset->to_text = [](const native::property_value &value) {
    return std::to_string(std::any_cast<int>(std::get<std::any>(value))) + " px";
};
preset->create_content = [](const native::property_value &,
                           native::property_drop_down::commit commit) {
    auto button = std::make_unique<native::button>("Use 24 px");
    button->on_click.connect([commit] {
        commit(std::any(24));
        return true;
    });
    return button;
};
_properties.add_item({"preset", "Preset", native::property_kind::drop_down,
                      std::any(16), {}, false, preset});
```

The converter formats the closed value. The callback commits and closes the
popup; cancelling preserves the old value. A returned panel or canvas can own
its own children. Removing the property cancels its popup and invalidates its
retained commit callback. Arbitrary values use the `std::any` alternative of
`property_value`; built-in text, numbers, and booleans use their direct types.

Read-only properties still accept programmatic changes. Duplicate IDs,
wrong value types, nonfinite numbers, and choices outside their list are
rejected. `set_row_height(0)` restores the theme/font default;
`set_label_width()` changes the label column.

For ruler and border configuration, continue with
[Chapter 17](17-INPUT-DIALOGS-AND-WINDOW-CHROME.md#rulers-and-status-bar).
For the larger demonstration, open Vision's **Properties and toolbars** window.

On Motif, plain property values use black text while dark native choice fields
keep their theme colors. A custom-content dropdown uses the same native
selection-only combo enclosure and arrow as a choice property; only its popup
contents differ. Embedded custom fields inherit the row's border policy.

Toolbar labels and tinted monochrome icons use
`theme::get_toolbar_text_color(state)`. GTK resolves this from the native
button style for idle, hover, held, selected and disabled states, matching
the tool's button background in light and dark themes. The default theme
implementation retains menu-style toolbar foregrounds.

GTK paints the empty tool bezel from its native CSS background and frame at
the actual item size, so left and right strips keep the complete button frame
at narrow widths. The toolbar draws its images and labels within that bezel.


Motif property values use the same control font across text, number, boolean,
choice and custom-dropdown rows. Normal values have black ink and matching
light entry backgrounds; their labels also use black ink. Ordinary controls
outside the inspector retain the desktop's native resources.


## New-feature acceptance window

Run `vision --new-features` from any backend build (through its documented
SDK session or native desktop). This public-API window exercises the recent
resource, document-input and chrome additions together:

1. Read the resource profile at the top. X11/GEMix choose monochrome artwork;
   color backends choose the default blue artwork. The keyboard capability
   is also shown using `app::get_unicode_text_input_supported()`. Both icons come from one named
   `assets.icons.platform-icon` resource with explicit `icon` metadata.
2. Type text into the canvas, including accented text where the backend
   supports composition. Escape consumes its physical key and clears the text.
   GEM's current text path remains ASCII-only.
3. Choose an item in the hosted combo. Use **Test → Focus canvas** or Ctrl+F,
   then type again. Check Shift-click and double-click counts in the status bar
   and the mirrored canvas feedback.
4. Drag the ruler triangles and tab marker. The block is fixed. Values snap
   to 0.1; release commits the final value. Switching focus during a drag must
   stop it, with no jump when the pointer returns.
5. Toggle hosted controls from the Test menu twice. The combo disappears and
   returns with its selection retained, without old toolbar buttons covering it.
   The icon-only tool stays square and returns keyboard focus to the canvas.
6. Compare 24-pixel line-height and em text. Some fonts, including the bundled
   terminal face, legitimately have equal metrics for these conventions.

The font fixture is the existing attributed `retro-terminal/terminal.ttf`,
which normal builds and deployment scripts copy beside Vision. Keep its
license files with it. The window constructs and round-trips its own small
PNG resource pack in memory; it needs no extra icon files.

`vision --new-features --features-smoke` creates the real window, checks
resource decode, font availability/measurement, native control creation,
removal/recreation and retained selection, canvas focus/UTF-8 dispatch and
square icon geometry, then exits with a nonzero status on failure. CTest
registers this as `native_new_features_smoke`. This smoke mode calls portable
input hooks; it is not proof of physical keyboard, IME or pointer delivery.
The backend keyboard/menu tests and the manual steps above cover those
separate paths. See the [feature matrix](../book-of-native/FEATURE-MATRIX.md)
for the actual target results.

Hosted control objects must outlive their toolbar. Removing their descriptors
destroys and detaches native children but does not delete C++ objects. A live
control from another parent cannot be adopted; destroy its native resource
first. A control cannot occupy two slots in the same toolbar.
