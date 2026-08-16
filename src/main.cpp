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
        // Construct the application window and connect its event
        // handlers.
        vision_window()
            : native::app_wnd("Vision", 100, 100, 640, 420)
            , _action("Activate", 24, 24, 120, 32)
            , _enabled("Enabled", 24, 70, 120, 24)
            , _compact("Compact", 24, 104, 120, 24)
            , _detailed("Detailed", 24, 132, 120, 24)
            , _choices(
                  {"First", "Second", "Third"}, 180, 24, 180, 132) {
            on_wnd_create.connect(this, &vision_window::on_create);
            on_wnd_paint.connect(this, &vision_window::on_paint);
            _action.on_click.connect(this, &vision_window::on_action);
            _enabled.on_change.connect(this,
                                       &vision_window::on_enabled);
            _compact.on_change.connect(this, &vision_window::on_radio);
            _detailed.on_change.connect(this, &vision_window::on_radio);
            _choices.on_selection_change.connect(
                this, &vision_window::on_selection);
            _compact.set_selected(true);
            _choices.set_selected_index(0);
        }

    private:
        native::button _action;
        native::check _enabled;
        native::radio _compact;
        native::radio _detailed;
        native::list _choices;
        int _activation_count = 0;

        // Create the native child control after the main window exists.
        bool on_create() {
            _action.set_parent(this);
            _action.create();
            _action.show();

            _enabled.set_parent(this);
            _enabled.create();
            _enabled.show();

            _compact.set_parent(this);
            _compact.create();
            _compact.show();

            _detailed.set_parent(this);
            _detailed.create();
            _detailed.show();

            _choices.set_parent(this);
            _choices.create();
            _choices.show();
            return true;
        }

        // Record an activation and request updated window content.
        bool on_action() {
            ++_activation_count;
            invalidate();
            return true;
        }

        bool on_enabled(bool) {
            invalidate();
            return true;
        }

        bool on_radio(bool) {
            invalidate();
            return true;
        }

        bool on_selection(int) {
            invalidate();
            return true;
        }

        // Draw the initial application status in the client area.
        bool on_paint(native::wnd_paint_event event) {
            event.g.set_ink(native::rgba(0, 0, 0, 255));
            event.g.draw_text(
                "Vision is built with the native library.",
                native::point(24, 190));
            event.g.draw_text("Activations: " +
                                  std::to_string(_activation_count),
                              native::point(24, 216));
            event.g.draw_text(
                std::string("Enabled: ") +
                    (_enabled.get_checked() ? "yes" : "no"),
                native::point(24, 242));
            event.g.draw_text(
                std::string("Mode: ") +
                    (_compact.get_selected() ? "compact" : "detailed"),
                native::point(24, 268));
            event.g.draw_text(
                "Selected item: " +
                    std::to_string(_choices.get_selected_index()),
                native::point(24, 294));
            return true;
        }
    };
} // namespace

int program(int, char **) {
    vision_window window;
    return native::app::run(window);
}
