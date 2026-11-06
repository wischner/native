//
// Drives ruler markers through the pointer, hosts controls in a toolbar,
// and checks the canvas text, focus and click contracts. Nothing here
// names a toolkit, so it runs on every backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include <native/canvas.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
    using namespace native;

    void expect(bool valid, const char *message) {
        if (!valid)
            throw std::runtime_error(message);
    }

    class window final : public app_wnd
    {
    public:
        window()
            : app_wnd("chrome", 30, 30, 480, 320),
              bar_(*this, window_edge::top, 32),
              ruler_(surface_, window_edge::top, 22) {
            tool_item choice;
            choice.id = "choice";
            choice.kind = tool_kind::control;
            choice.control = &box_;
            choice.length = 120;
            bar_.add_item(choice);
            ruler_.set_units_per_pixel(0.01).set_marker_snap(0.25);
            on_wnd_create.connect([this] {
                surface_.set_parent(this);
                surface_.create();
                surface_.set_bounds(get_client_bounds());
                surface_.show();
                app::post([this] { run(); });
                return true;
            });
        }

        bool failed = false;

    private:
        void run() {
            try {
                check_toolbar();
                check_ruler();
                check_canvas();
            } catch (const std::exception &error) {
                std::cerr << "FAILED: " << error.what() << '\n';
                failed = true;
            }
            destroy();
        }

        void check_toolbar() {
            expect(box_.get_created(), "the toolbar creates its control");
            expect(box_.get_parent() != nullptr, "the control is parented");
            expect(box_.get_dimensions().w == 120,
                   "the toolbar places its control");
            bool threw = false;
            try {
                bar_.add_item({"empty", "", tool_kind::control});
            } catch (const std::invalid_argument &) {
                threw = true;
            }
            expect(threw, "a control tool needs a control");
        }

        void check_ruler() {
            ruler_.set_active_range(1.0, 3.0);
            ruler_.set_markers(
                {{7, 2.0, ruler_marker_shape::down_triangle, true},
                 {8, 3.5, ruler_marker_shape::block, false}});
            std::vector<ruler_marker_move> moves;
            ruler_.on_marker_move.connect([&](ruler_marker_move move) {
                moves.push_back(move);
                return true;
            });
            bool client_saw_press = false;
            surface_.on_mouse_click.connect([&](mouse_event) {
                client_saw_press = true;
                return true;
            });
            const rect strip = ruler_.get_bounds();
            const point on_marker(static_cast<coord>(strip.x1() + 200),
                                  static_cast<coord>(strip.y1() + 4));
            surface_.on_native_mouse_click(mouse_event(
                mouse_button::left, mouse_action::press, on_marker));
            expect(!client_saw_press, "a marker press stays in the ruler");
            const point moved(static_cast<coord>(strip.x1() + 248),
                              static_cast<coord>(strip.y1() + 60));
            surface_.on_native_mouse_move(moved);
            surface_.on_native_mouse_click(mouse_event(
                mouse_button::left, mouse_action::release, moved));
            expect(moves.size() == 2, "a drag reports move and release");
            expect(moves.back().id == 7 && moves.back().finished,
                   "release finishes the gesture");
            expect(moves.back().value == 2.5, "drags snap to the interval");
            moves.clear();
            const point fixed(static_cast<coord>(strip.x1() + 350),
                              static_cast<coord>(strip.y1() + 4));
            surface_.on_native_mouse_click(mouse_event(
                mouse_button::left, mouse_action::press, fixed));
            surface_.on_native_mouse_click(mouse_event(
                mouse_button::left, mouse_action::release, fixed));
            expect(moves.empty(), "a fixed marker cannot be dragged");
        }

        void check_canvas() {
            surface_.focus();
            expect(surface_.get_focused(), "focus() takes the keyboard");
            std::string typed;
            surface_.on_text_input.connect([&](const std::string &utf8) {
                typed += utf8;
                return true;
            });
            surface_.on_native_text_input("\xc5\xbe");
            expect(typed == "\xc5\xbe", "text reaches the canvas");
            int clicks = 0;
            key_modifiers held = key_modifiers::none;
            surface_.on_mouse_click.connect([&](mouse_event event) {
                clicks = event.clicks;
                held = event.modifiers;
                return true;
            });
            const point inside(40, 120);
            surface_.on_native_key({key_code::left_shift, key_action::press,
                                    false, key_modifiers::shift});
            for (int press = 1; press <= 2; ++press) {
                surface_.on_native_mouse_click(mouse_event(
                    mouse_button::left, mouse_action::press, inside));
                expect(clicks == press, "the canvas counts quick presses");
                expect((held & key_modifiers::shift) == key_modifiers::shift,
                       "held modifiers reach the press");
                surface_.on_native_mouse_click(mouse_event(
                    mouse_button::left, mouse_action::release, inside));
            }
            surface_.on_native_key({key_code::left_shift, key_action::release,
                                    false, key_modifiers::none});
        }

        canvas surface_{0, 0, 480, 240};
        combo_box box_{{"one", "two"}};
        toolbar bar_;
        ruler ruler_;
    };
} // namespace

int program(int, char **) {
    window main_window;
    const auto result = app::run(main_window);
    return result ? result : main_window.failed ? 1 : 0;
}
