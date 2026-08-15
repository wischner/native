# Chapter 4: Buttons and Control Lifecycle

A control is a C++ object whose native resource is created after its parent
window exists. Keep controls as members of the window so their lifetime
matches the parent.

## The creation sequence

Connect to `on_wnd_create` and perform these steps for each child control:

1. Assign the parent with `set_parent()`.
2. Create the backend resource with `create()`.
3. Make the control visible with `show()`.

The control's signals can be connected before its backend resource exists.

```cpp
//
// Demonstrates a native button and its complete control lifecycle.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <string>

#include <native.h>

class button_window : public native::app_wnd
{
public:
    // Construct the window, button, and event connections.
    button_window()
        : native::app_wnd(
              "Button Example", 100, 100, 420, 240),
          _button("Click me", 20, 20, 120, 32) {
        on_wnd_create.connect(
            this, &button_window::on_create);
        on_wnd_paint.connect(
            this, &button_window::on_paint);
        _button.on_click.connect(
            this, &button_window::on_button_click);
    }

private:
    native::button _button;
    int _click_count = 0;

    // Attach and create child controls after the parent exists.
    bool on_create() {
        _button.set_parent(this);
        _button.create();
        _button.show();
        return true;
    }

    // Update application state when the button is activated.
    bool on_button_click() {
        ++_click_count;
        invalidate();
        return true;
    }

    // Draw state that is not represented by a native control.
    bool on_paint(native::wnd_paint_event event) {
        event.g.set_ink(native::rgba(0, 0, 0, 255));
        const std::string message =
            "Button clicks: " + std::to_string(_click_count);
        event.g.draw_text(message, native::point(20, 80));
        event.g.draw_text(
            "Press the button above.", native::point(20, 110));
        return true;
    }
};

// Create the button window and enter the native event loop.
int program(int, char **) {
    button_window window;
    return native::app::run(window);
}
```

The C++ window owns `_button`. The parent relationship maintained by
`set_parent()` is non-owning and is used for backend creation, layout, and
event routing.

Next: [Configuring controls](05-configuring-controls.md).
