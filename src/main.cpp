//
// Implements the Vision application entry and its initial main window.
// The program uses only the public native library interface.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <string>

#include <native.h>

namespace
{
    class vision_window final : public native::app_wnd
    {
    public:
        // Construct the application window and connect its event handlers.
        vision_window()
            : native::app_wnd("Vision", 100, 100, 640, 420),
              _action("Activate", 24, 24, 120, 32) {
            on_wnd_create.connect(
                this, &vision_window::on_create);
            on_wnd_paint.connect(
                this, &vision_window::on_paint);
            _action.on_click.connect(
                this, &vision_window::on_action);
        }

    private:
        native::button _action;
        int _activation_count = 0;

        // Create the native child control after the main window exists.
        bool on_create() {
            _action.set_parent(this);
            _action.create();
            _action.show();
            return true;
        }

        // Record an activation and request updated window content.
        bool on_action() {
            ++_activation_count;
            invalidate();
            return true;
        }

        // Draw the initial application status in the client area.
        bool on_paint(native::wnd_paint_event event) {
            event.g.set_ink(native::rgba(0, 0, 0, 255));
            event.g.draw_text(
                "Vision is built with the native library.",
                native::point(24, 90));
            event.g.draw_text(
                "Activations: " +
                    std::to_string(_activation_count),
                native::point(24, 116));
            return true;
        }
    };
}

int program(int, char **) {
    vision_window window;
    return native::app::run(window);
}
