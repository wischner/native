# Chapter 5: Configuring Controls

Native controls offer several geometry constructors and can be changed after
creation. This chapter combines those forms in one window.

## Geometry constructors

A button can receive:

- scalar `x`, `y`, `width`, and `height` values;
- a `native::point` and `native::size`; or
- a complete `native::rect`.

When dimensions are omitted, the button uses its default size. All forms
initialize the same cached bounds.

## Runtime changes

`button::set_text()` updates both the cached label and an existing backend
control. `app_wnd::set_title()` does the same for the application window.

```cpp
//
// Demonstrates button constructors and runtime property changes.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <string>

#include <native.h>

class button_configurations_window : public native::app_wnd
{
public:
    // Construct every control configuration and connect its events.
    button_configurations_window()
        : native::app_wnd(
              "Button Configurations", 100, 100, 560, 320),
          _default_size("Default size", 20, 20),
          _point_size(
              "Point + size",
              native::point(150, 20),
              native::size(140, 32)),
          _rect_constructor(
              "Rect constructor",
              native::rect(310, 20, 140, 32)),
          _rename_button("Rename first", 20, 70, 130, 32),
          _title_button("Update title", 170, 70, 130, 32) {
        on_wnd_create.connect(
            this, &button_configurations_window::on_create);
        on_wnd_paint.connect(
            this, &button_configurations_window::on_paint);

        _default_size.on_click.connect(
            this,
            &button_configurations_window::on_default_click);
        _point_size.on_click.connect(
            this,
            &button_configurations_window::on_point_click);
        _rect_constructor.on_click.connect(
            this,
            &button_configurations_window::on_rect_click);
        _rename_button.on_click.connect(
            this,
            &button_configurations_window::on_rename_click);
        _title_button.on_click.connect(
            this,
            &button_configurations_window::on_title_click);
    }

private:
    native::button _default_size;
    native::button _point_size;
    native::button _rect_constructor;
    native::button _rename_button;
    native::button _title_button;

    int _total_clicks = 0;
    int _rename_count = 0;

    // Attach, create, and show one child button.
    void attach_button(native::button &button) {
        button.set_parent(this);
        button.create();
        button.show();
    }

    // Create all controls after the application window is ready.
    bool on_create() {
        attach_button(_default_size);
        attach_button(_point_size);
        attach_button(_rect_constructor);
        attach_button(_rename_button);
        attach_button(_title_button);
        return true;
    }

    // Count activation of the default-size button.
    bool on_default_click() {
        ++_total_clicks;
        invalidate();
        return true;
    }

    // Change the point-and-size button after its first activation.
    bool on_point_click() {
        ++_total_clicks;
        _point_size.set_text("Point + size [clicked]");
        invalidate();
        return true;
    }

    // Change the rectangle-constructed button after activation.
    bool on_rect_click() {
        ++_total_clicks;
        _rect_constructor.set_text("Rect [clicked]");
        invalidate();
        return true;
    }

    // Give the first button an incrementing label.
    bool on_rename_click() {
        ++_total_clicks;
        ++_rename_count;
        _default_size.set_text(
            "Default #" + std::to_string(_rename_count));
        invalidate();
        return true;
    }

    // Reflect the click count in the native window title.
    bool on_title_click() {
        ++_total_clicks;
        set_title(
            "Button Configurations (" +
            std::to_string(_total_clicks) + ")");
        invalidate();
        return true;
    }

    // Explain the active configuration and display its click count.
    bool on_paint(native::wnd_paint_event event) {
        event.g.set_ink(native::rgba(0, 0, 0, 255));
        event.g.draw_text(
            "Configuration demo:", native::point(20, 130));
        event.g.draw_text(
            "- constructor with default dimensions",
            native::point(20, 155));
        event.g.draw_text(
            "- constructor with point and size",
            native::point(20, 178));
        event.g.draw_text(
            "- constructor with rectangle",
            native::point(20, 201));
        event.g.draw_text(
            "- runtime text and title changes",
            native::point(20, 224));
        event.g.draw_text(
            "Total clicks: " + std::to_string(_total_clicks),
            native::point(20, 260));
        return true;
    }
};

// Create the configuration window and enter the native event loop.
int program(int, char **) {
    button_configurations_window window;
    return native::app::run(window);
}
```

Use setters rather than reaching into backend state. They preserve the public
model and let each backend perform the correct native update.

Next: [Absolute layout](06-ABSOLUTE-LAYOUT.md).
