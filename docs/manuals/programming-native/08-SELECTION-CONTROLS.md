# Chapter 8: Selection Controls

`check`, `radio`, and `list` are child windows with the same parent, creation,
visibility, geometry, and lifetime rules as `button`. Their state is cached in
the portable C++ object and synchronized with the active backend control.

## State and signals

| Control | Programmatic state | User-action signal |
| --- | --- | --- |
| `check` | `get_checked()` and `set_checked()` | `on_change(bool)` |
| `radio` | `get_selected()` and `set_selected()` | `on_change(bool)` |
| `list` | `get_selected_index()` and `set_selected_index()` | `on_selection_change(int)` |

A list index of `-1` means that no item is selected. `add_item()`,
`remove_item()`, `clear_items()`, and `set_items()` update the item model and
an already-created native control.
`items << "Compact" << "Detailed"` is append-only sugar for repeated
`add_item()` calls; the named operation remains available.

Radio controls with the same parent form one exclusive group. Selecting one
clears its radio siblings. Use a separate parent window when an interface
needs more than one independent group.

Setters do not emit user-action signals. This lets application code restore a
model without pretending that the user changed it.

## A complete selection example

```cpp
//
// Demonstrates check, sibling-exclusive radio, and list controls.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <string>
#include <vector>

#include <native.h>

class selection_window : public native::app_wnd
{
public:
    selection_window()
        : native::app_wnd(
              "Selection Controls", 100, 100, 460, 300),
          _remember("Remember choice", 20, 20, 180, 24),
          _compact("Compact", 20, 56, 140, 24),
          _detailed("Detailed", 20, 88, 140, 24),
          _choices(
              std::vector<std::string>{
                  "First", "Second", "Third"},
              220, 20, 180, 110) {
        on_wnd_create.connect(
            this, &selection_window::on_create);
        on_wnd_paint.connect(
            this, &selection_window::on_paint);

        _remember.on_change.connect(
            this, &selection_window::on_remember);
        _compact.on_change.connect(
            this, &selection_window::on_style);
        _detailed.on_change.connect(
            this, &selection_window::on_style);
        _choices.on_selection_change.connect(
            this, &selection_window::on_choice);

        _compact.set_selected(true);
        _choices.set_selected_index(0);
    }

private:
    native::check _remember;
    native::radio _compact;
    native::radio _detailed;
    native::list _choices;
    std::string _status = "First selected";

    void attach(native::wnd &control) {
        control.set_parent(this);
        control.create();
        control.show();
    }

    bool on_create() {
        attach(_remember);
        attach(_compact);
        attach(_detailed);
        attach(_choices);
        return true;
    }

    bool on_remember(bool checked) {
        _status = checked ? "Choice remembered" : "Memory cleared";
        invalidate();
        return true;
    }

    bool on_style(bool selected) {
        if (selected) {
            _status = _compact.get_selected()
                ? "Compact selected"
                : "Detailed selected";
            invalidate();
        }
        return true;
    }

    bool on_choice(int index) {
        _status = index < 0
            ? "No item selected"
            : _choices.get_items()[
                  static_cast<std::size_t>(index)] + " selected";
        invalidate();
        return true;
    }

    bool on_paint(native::wnd_paint_event event) {
        event.g.draw_text(_status, native::point(20, 170));
        return true;
    }
};

int program(int, char **) {
    selection_window window;
    return native::app::run(window);
}
```

The application owns all four C++ controls. The `attach()` helper borrows
them through their common `wnd` base without changing that ownership.

## Native controls and emulation

Native uses real controls where the platform or toolkit supplies them:

| Backend | Control implementation |
| --- | --- |
| X11 | [Athena Widgets](https://xorg.freedesktop.org/releases/X11R7.7/doc/libXaw/libXaw.html) `Toggle` and `List` widgets |
| OpenMotif | `XmToggleButton` and `XmList` |
| OPEN LOOK | XView `PANEL_CHECK_BOX`, `PANEL_CHOICE`, and `PANEL_LIST` items |
| Window Maker | WINGs switch/radio `WMButton` widgets and `WMList` |
| Windows | `BUTTON` and `LISTBOX` controls |
| Haiku | `BCheckBox`, `BRadioButton`, and `BListView` |
| macOS | `NSButton` and `NSTableView` |
| SDL2 | Backend-owned controls drawn with the SDL2 theme |
| GEMix | Backend-owned controls drawn with the GEM theme |

Window Maker keeps the native `WMList` widget, including its keyboard,
pointer, and WINGs scroller behavior. Its supported user-draw hook replaces
the stock white selected row with the same dark-gray selection and light-gray
selected text used by collection and table controls; ordinary rows use the
desktop panel gray. The Window Maker themed-list primitive uses those same
roles for custom surfaces.

SDL2 and GEMix own their hit testing, focus, state, and event integration;
they are not application-side painted substitutes. Custom surfaces can draw
the same visual parts through `native::theme`, which Chapter 10 covers. The
complete themed check/radio primitives use the same indicator geometry,
control font, panel background, and state colors as the live emulated
controls.

Next: [Owned windows and file dialogs](09-OWNED-WINDOWS-AND-DIALOGS.md).
