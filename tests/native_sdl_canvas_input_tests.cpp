//
// Routes SDL2 composed text and mouse transitions to a focused canvas,
// the input a document editor depends on.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include <native/canvas.h>
#include "../lib/native/toolkits/sdl2/globals.h"
#include <SDL2/SDL.h>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    using namespace native;

    void expect(bool valid, const char *message) {
        if (!valid)
            throw std::runtime_error(message);
    }

    class editor final : public app_wnd
    {
    public:
        editor() : app_wnd("canvas input", 40, 40, 320, 240) {
            surface_.set_parent(this);
            on_wnd_create.connect([this] {
                surface_.create();
                surface_.show();
                app::post([this] { run(); });
                return true;
            });
        }

        bool failed = false;

    private:
        void run() {
            try {
                check();
            } catch (const std::exception &error) {
                std::cerr << "FAILED: " << error.what() << '\n';
                failed = true;
            }
            destroy();
        }

        void check() {
            std::string text;
            surface_.on_text_input.connect(
                [&](const std::string &utf8) {
                    text += utf8;
                    return true;
                });
            mouse_event seen;
            surface_.on_mouse_click.connect([&](mouse_event event) {
                seen = event;
                return true;
            });
            expect(!linux::sdl2::handle_canvas_text(this, "x"),
                   "an unfocused canvas receives no text");
            expect(linux::sdl2::handle_canvas_mouse(
                       this, 20, 20, true, false),
                   "the press reaches the canvas");
            expect(seen.clicks == 1, "a first press counts one click");
            linux::sdl2::handle_canvas_mouse(this, 20, 20, false, true);
            surface_.on_native_key({key_code::left_shift, key_action::press,
                                    false, key_modifiers::shift});
            linux::sdl2::handle_canvas_mouse(this, 21, 20, true, false);
            expect(seen.clicks == 2, "a quick second press is a double click");
            expect((seen.modifiers & key_modifiers::shift) ==
                       key_modifiers::shift,
                   "held shift arrives with the press");
            linux::sdl2::handle_canvas_mouse(this, 21, 20, false, true);
            surface_.on_native_key({key_code::left_shift, key_action::release,
                                    false, key_modifiers::none});
            expect(linux::sdl2::handle_canvas_text(this, "é"),
                   "the focused canvas receives composed text");
            expect(text == "é", "UTF-8 is delivered unchanged");
            SDL_StopTextInput();
            surface_.focus();
            expect(SDL_IsTextInputActive(), "focus() restarts SDL text input");
            surface_.on_key.connect([](key_event event) {
                return event.key == key_code::a;
            });
            surface_.on_native_key({key_code::a, key_action::press,
                                    false, key_modifiers::none});
            linux::sdl2::handle_canvas_text(this, "a");
            expect(text == "é", "consumed presses suppress separate SDL text");
        }

        canvas surface_{10, 10, 200, 150};
    };
} // namespace

int program(int, char **) {
    editor window;
    const auto result = app::run(window);
    return result ? result : window.failed ? 1 : 0;
}
