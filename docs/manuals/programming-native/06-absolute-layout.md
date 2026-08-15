# Chapter 6: Absolute Layout

An `absolute_layout_manager` registers child windows without changing their
explicit coordinates. It is useful for fixed-size interfaces and for code
that calculates bounds itself.

## Installing a layout

The application owns its layout through `std::unique_ptr`. Controls remain
owned by the window class. A layout borrows the controls it arranges.

Children can be registered through `add()` or the stream-style `operator<<`.
Both forms have identical behavior.

```cpp
//
// Demonstrates explicit control placement with absolute layout.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <memory>
#include <string>

#include <native.h>

class layout_absolute_window : public native::app_wnd
{
public:
    // Construct explicitly positioned controls and their events.
    layout_absolute_window()
        : native::app_wnd(
              "Layout: Absolute", 100, 100, 520, 280),
          _left("Left", 20, 20, 120, 32),
          _middle("Middle", 180, 70, 140, 32),
          _right("Right", 360, 140, 120, 32) {
        on_wnd_create.connect(
            this, &layout_absolute_window::on_create);
        on_wnd_paint.connect(
            this, &layout_absolute_window::on_paint);

        _left.on_click.connect(
            this, &layout_absolute_window::on_any_click);
        _middle.on_click.connect(
            this, &layout_absolute_window::on_any_click);
        _right.on_click.connect(
            this, &layout_absolute_window::on_any_click);
    }

private:
    native::button _left;
    native::button _middle;
    native::button _right;
    int _clicks = 0;

    // Create one child button for the application window.
    void create_button(native::button &button) {
        button.set_parent(this);
        button.create();
        button.show();
    }

    // Create controls and install their fixed-coordinate layout.
    bool on_create() {
        create_button(_left);
        create_button(_middle);
        create_button(_right);

        auto layout =
            std::make_unique<native::absolute_layout_manager>();
        layout->add(_left);
        (*layout) << _middle << _right;
        set_layout(std::move(layout));
        return true;
    }

    // Count clicks from any of the registered buttons.
    bool on_any_click() {
        ++_clicks;
        invalidate();
        return true;
    }

    // Describe the layout and display the shared click count.
    bool on_paint(native::wnd_paint_event event) {
        event.g.set_ink(native::rgba(0, 0, 0, 255));
        event.g.draw_text(
            "Absolute layout preserves explicit coordinates.",
            native::point(20, 205));
        event.g.draw_text(
            "Classic and stream registration can be combined.",
            native::point(20, 225));
        event.g.draw_text(
            "Clicks: " + std::to_string(_clicks),
            native::point(20, 245));
        return true;
    }
};

// Create the absolute-layout window and enter the native event loop.
int program(int, char **) {
    layout_absolute_window window;
    return native::app::run(window);
}
```

Resizing the parent triggers `relayout()`, but the absolute manager preserves
the bounds originally assigned to each control.

Next: [Grid and nested layout](07-grid-layout.md).
