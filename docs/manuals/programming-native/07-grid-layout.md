# Chapter 7: Grid and Nested Layout

A `grid_layout_manager` divides available space into rows and columns. Tracks
can have fixed pixel sizes or weighted shares of the remaining space.

## Track sizing

- `native::pixels(value)` creates a fixed-size track.
- `native::star(weight)` creates a weighted track.
- A larger star weight receives a larger share of remaining space.

`native::cell()` places a control by row, column, span, and margin. A nested
grid can occupy a parent cell through `native::child_grid()`.

```cpp
//
// Demonstrates responsive and nested grid layout with native controls.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <memory>
#include <string>

#include <native.h>

class layout_grid_window : public native::app_wnd
{
public:
    // Construct the controls and connect their shared event handler.
    layout_grid_window()
        : native::app_wnd(
              "Layout: Grid", 100, 100, 760, 440),
          _toolbar("Toolbar", 0, 0, 120, 32),
          _left_panel("Left Panel", 0, 0, 120, 32),
          _status("Status", 0, 0, 120, 32),
          _grid_1("G1", 0, 0, 80, 28),
          _grid_2("G2", 0, 0, 80, 28),
          _grid_3("G3", 0, 0, 80, 28),
          _grid_4("G4", 0, 0, 80, 28) {
        on_wnd_create.connect(
            this, &layout_grid_window::on_create);
        on_wnd_paint.connect(
            this, &layout_grid_window::on_paint);

        connect_button(_toolbar);
        connect_button(_left_panel);
        connect_button(_status);
        connect_button(_grid_1);
        connect_button(_grid_2);
        connect_button(_grid_3);
        connect_button(_grid_4);
    }

private:
    native::button _toolbar;
    native::button _left_panel;
    native::button _status;
    native::button _grid_1;
    native::button _grid_2;
    native::button _grid_3;
    native::button _grid_4;
    int _clicks = 0;

    // Route a button's activation to the shared handler.
    void connect_button(native::button &button) {
        button.on_click.connect(
            this, &layout_grid_window::on_any_click);
    }

    // Create one child button for the application window.
    void create_button(native::button &button) {
        button.set_parent(this);
        button.create();
        button.show();
    }

    // Create controls and install the root and nested grids.
    bool on_create() {
        create_button(_toolbar);
        create_button(_left_panel);
        create_button(_status);
        create_button(_grid_1);
        create_button(_grid_2);
        create_button(_grid_3);
        create_button(_grid_4);

        auto root =
            std::make_unique<native::grid_layout_manager>();

        (*root)
            << native::row(native::pixels(48))
            << native::row(native::star())
            << native::row(native::pixels(52))
            << native::column(native::star(1.0f))
            << native::column(native::star(2.0f))
            << native::cell(_toolbar, 0, 0, 1, 2, 8)
            << native::cell(_left_panel, 1, 0, 1, 1, 8)
            << native::cell(_status, 2, 0, 1, 2, 8);

        auto child =
            std::make_unique<native::grid_layout_manager>(2, 2);
        child->add(_grid_1, 0, 0, 1, 1, 8)
             .add(_grid_2, 0, 1, 1, 1, 8)
             .add(_grid_3, 1, 0, 1, 1, 8)
             .add(_grid_4, 1, 1, 1, 1, 8);

        (*root) << native::child_grid(
            std::move(child), 1, 1, 1, 1, 4);

        set_layout(std::move(root));
        return true;
    }

    // Count activations and update the status control.
    bool on_any_click() {
        ++_clicks;
        _status.set_text(
            "Status: clicks=" + std::to_string(_clicks));
        invalidate();
        return true;
    }

    // Explain the resize behavior supplied by the grid.
    bool on_paint(native::wnd_paint_event event) {
        event.g.set_ink(native::rgba(0, 0, 0, 255));
        event.g.draw_text(
            "Grid layout uses star and pixel tracks.",
            native::point(16, 74));
        event.g.draw_text(
            "Resize the window to see automatic layout.",
            native::point(16, 94));
        return true;
    }
};

// Create the grid-layout window and enter the native event loop.
int program(int, char **) {
    layout_grid_window window;
    return native::app::run(window);
}
```

## Layout ownership

The root layout owns the nested grid. Both layouts borrow their control
pointers. The window still owns every `native::button`, so controls outlive
the layouts that reference them.

This exhausts the former runnable examples. Return to the
[manual contents](../PROGRAMMING-NATIVE.md) or continue with the internal
[Book of Native](../BOOK-OF-NATIVE.md) for implementation details.
