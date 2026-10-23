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
