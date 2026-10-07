//
// Demonstrates packaged CRT passes beside a native text editor, with scoped
// pointer capture, hover hiding, cancellation, and an explicit escape action.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "vision_mouse_shaders.h"
#include "shader_demo_data.h"
#include <native.h>
#include <optional>

namespace
{
    class mouse_shader_window : public native::app_wnd
    {
    public:
        mouse_shader_window()
            : app_wnd("Native mouse and programmable image shaders", 60, 60, 640, 520)
            , _editor("The image uses three portable shader passes. This is a native editor.\n"
                "Click the image first; use the menu for color, hiding or capture. Escape releases capture.",
                native::text_edit_mode::multi_line) {
            _view.set_parent(this);
            _editor.set_parent(this);
            auto layout = std::make_unique<native::grid_layout_manager>();
            (*layout) << native::row(native::star()) << native::row(native::pixels(100))
                << native::column(native::star()) << native::cell(_view, 0, 0)
                << native::cell(_editor, 1, 0);
            set_layout(std::move(layout));
            native::menu_items_proxy effects;
            effects << std::pair<int, std::string>{1, "&Color"}
                << std::pair<int, std::string>{2, "&Amber"}
                << std::pair<int, std::string>{3, "&Green"}
                << std::pair<int, std::string>{4, "&Original image"}
                << std::pair<int, std::string>{5, "&Animate / source updates"};
            native::menu_items_proxy pointer;
            pointer << std::pair<int, std::string>{6, "Toggle hover &hiding"}
                << std::pair<int, std::string>{7, "Capture &relative motion"}
                << std::pair<int, std::string>{8, "&Release capture\tEsc"};
            menu << "&Effects" << effects << "&Pointer" << pointer;
            on_wnd_create.connect([this] { create_children(); return false; });
            on_menu.connect([this](int command) { return menu_command(command); });
            const auto key = [this](native::key_event event) {
                if (event.key == native::key_code::escape && event.action == native::key_action::press) {
                    _capture.reset(); return true;
                }
                return false;
            };
            on_key.connect(key);
            _view.on_key.connect(key);
            _view.on_mouse_cancel.connect([this](native::mouse_cancel_reason) {
                _capture.reset(); set_title("Pointer cancelled; capture requires a new gesture."); return false;
            });
            _view.on_mouse_enter.connect([this](native::point) {
                set_title("Image client: hidden policy is confined to this child."); return false;
            });
            _view.on_mouse_leave.connect([this](native::point) {
                set_title("Outside image client: ordinary editor/menu cursor."); return false;
            });
            _view.on_mouse_motion.connect([this](native::mouse_motion_event event) {
                if (event.kind == native::mouse_motion_kind::relative)
                    set_title("Relative dx=" + std::to_string(event.dx) + " dy=" +
                        std::to_string(event.dy) + " (Escape releases)");
                return false;
            });
            _view.on_shader_error.connect([this](const std::string &error) {
                _editor.set_text(error); return false;
            });
        }
    private:
        void create_children() {
            _view.create(); _view.show();
            _editor.create(); _editor.show();
            native::img source(256, 192);
            for (int y = 0; y < source.h(); ++y) for (int x = 0; x < source.w(); ++x) {
                const bool checker = ((x / 16) + (y / 16)) % 2;
                source.pixels()[y * source.w() + x] = native::rgba(
                    checker ? 240 : x, checker ? y : 48, checker ? 96 : 220, 255);
            }
            _view.set_source(source, 1, 0);
            _effect = native::shader_package::decode(std::as_bytes(std::span(
                native_crt_package, sizeof(native_crt_package) - 1)));
            std::string error;
            if (!_view.set_effect(_effect, error)) _editor.set_text(error);
        }
        bool menu_command(int command) {
            if (command >= 1 && command <= 3) {
                std::string error;
                if (!_view.get_effect().get_valid() && !_view.set_effect(_effect, error)) {
                    _editor.set_text(error); return true;
                }
                const std::array<float, 4> color = command == 2
                    ? std::array<float, 4>{1, 0.65f, 0.2f, 1}
                    : command == 3 ? std::array<float, 4>{0.2f, 1, 0.25f, 1}
                    : std::array<float, 4>{1, 1, 1, 1};
                _view.set_parameter("tint", color);
            } else if (command == 4) _view.clear_effect();
            else if (command == 5) _view.set_animation(
                _view.get_animation() == native::shader_animation::continuous
                    ? native::shader_animation::source_updates : native::shader_animation::continuous);
            else if (command == 6) _view.set_cursor(
                _view.get_cursor() == native::mouse_cursor::hidden
                    ? native::mouse_cursor::arrow : native::mouse_cursor::hidden);
            else if (command == 7) {
                std::string error;
                _capture = _view.capture_mouse({native::mouse_capture_mode::relative, false}, error);
                if (!_capture) _editor.set_text(error);
            } else if (command == 8) _capture.reset();
            return true;
        }
        native::shader_view _view;
        native::text_edit _editor;
        native::shader_package _effect;
        std::optional<native::mouse_capture> _capture;
    };
}
namespace vision
{
    int run_mouse_shaders() {
        mouse_shader_window window;
        return native::app::run(window);
    }
}
