//
// Tests real SDL event dispatch and audio ownership across GUI-loop teardown.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>
#include "toolkits/sdl2/globals.h"
#include <SDL.h>
#include "../lib/native/input_state.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace
{
    class input_window final : public native::app_wnd
    {
    public:
        int presses = 0, releases = 0, repeats = 0, resets = 0;
        native::text_edit editor{"", native::text_edit_mode::single_line, 10, 10, 200, 30};
        native::text_edit second{"", native::text_edit_mode::single_line, 10, 50, 200, 30};
        native::text_edit omitted{"", native::text_edit_mode::single_line, 10, 90, 200, 30};
        native::audio_out audio{44100, 1, 22050};
        bool failed = false;
        native::button action{"Action"};
        native::check option{"Option"};
        int activations = 0;
        input_window() : native::app_wnd("Keyboard test") {
            on_key.connect([this](native::key_event event) {
                if (event.action == native::key_action::press) {
                    ++presses; if (event.repeat) ++repeats;
                } else ++releases;
                return true;
            });
            on_key_reset.connect([this] { ++resets; return true; });
            on_wnd_create.connect([this] {
                editor.set_parent(this).create(); editor.show();
                second.set_parent(this).create(); second.show();
                omitted.set_parent(this).create(); omitted.show();
                editor.set_tab_order(2); second.set_tab_order(1);
                action.set_parent(this).create(); action.show();
                option.set_parent(this).create(); option.show();
                action.on_click.connect([this] { ++activations; return true; });
                check(audio.open(), "SDL dummy audio failed to open.");
                native::app::post([this] { sequence(); });
                return true;
            });
        }
    private:
        void check(bool valid, const char *message) {
            if (!valid) { failed = true; std::cerr << message << '\n'; }
        }
        Uint32 id() {
            return SDL_GetWindowID(linux::sdl2::wnd_bindings.handle_from_object(this));
        }
        void key(Uint32 type, SDL_Scancode scan, bool repeat = false) {
            SDL_Event event{}; event.type = type; event.key.windowID = id();
            event.key.keysym.scancode = scan;
            event.key.keysym.sym = SDL_GetKeyFromScancode(scan);
            event.key.repeat = repeat; SDL_PushEvent(&event);
        }
        void focus(Uint8 type) {
            SDL_Event event{}; event.type = SDL_WINDOWEVENT;
            event.window.windowID = id(); event.window.event = type;
            SDL_PushEvent(&event);
        }
        void traversal_sequence() {
            editor.focus();
            key(SDL_KEYDOWN, SDL_SCANCODE_TAB);
            key(SDL_KEYUP, SDL_SCANCODE_TAB);
            native::app::post([this] {
                check(native::detail::input_target(*this) == &second,
                      "SDL Tab did not wrap past the omitted editor.");
                key(SDL_KEYDOWN, SDL_SCANCODE_RETURN);
                key(SDL_KEYUP, SDL_SCANCODE_RETURN);
                native::app::post([this] {
                    check(native::detail::input_target(*this) == &editor,
                          "SDL Enter-as-Tab did not reach the next editor.");
                    check(editor.get_text().empty() && second.get_text().empty(),
                          "Traversal inserted text in an editor.");
                    editor.set_enter_as_tab(false);
                    key(SDL_KEYDOWN, SDL_SCANCODE_RETURN);
                    key(SDL_KEYUP, SDL_SCANCODE_RETURN);
                    native::app::post([this] {
                        check(native::detail::input_target(*this) == &editor,
                              "Disabled Enter-as-Tab moved focus.");
                        action.focus();
                        key(SDL_KEYDOWN, SDL_SCANCODE_RETURN);
                        key(SDL_KEYUP, SDL_SCANCODE_RETURN);
                        native::app::post([this] {
                            check(activations == 1, "Enter did not activate the focused button once.");
                            option.focus();
                            key(SDL_KEYDOWN, SDL_SCANCODE_SPACE);
                            key(SDL_KEYUP, SDL_SCANCODE_SPACE);
                            native::app::post([this] {
                                check(option.get_checked(), "Space did not toggle the focused check.");
                                destroy();
                            });
                        });
                    });
                });
            });
        }
        void sequence() {
            focus(SDL_WINDOWEVENT_FOCUS_GAINED);
            key(SDL_KEYDOWN, SDL_SCANCODE_A);
            key(SDL_KEYDOWN, SDL_SCANCODE_A, true);
            key(SDL_KEYUP, SDL_SCANCODE_A);
            key(SDL_KEYDOWN, SDL_SCANCODE_LSHIFT);
            key(SDL_KEYDOWN, SDL_SCANCODE_RSHIFT);
            focus(SDL_WINDOWEVENT_FOCUS_LOST);
            key(SDL_KEYUP, SDL_SCANCODE_LSHIFT);
            native::app::post([this] {
                check(presses == 4 && releases == 1 && repeats == 1 && resets == 1,
                    "Real SDL dispatch press/release/repeat/reset mismatch.");
                auto *binding = linux::sdl2::text_edit_bindings.object_from_handle(&editor);
                binding->focused = true; editor.on_native_focus(true);
                focus(SDL_WINDOWEVENT_FOCUS_GAINED);
                // Window activation must not clear logical child focus.
                editor.on_native_focus(true);
                key(SDL_KEYDOWN, SDL_SCANCODE_B);
                key(SDL_KEYUP, SDL_SCANCODE_B);
                native::app::post([this] {
                    check(presses == 4, "Editor physical input leaked to the root.");
                    traversal_sequence();
                });
            });
        }
    };
}
int main() {
    input_window window;
    native::app::run(window);
    // Native loop shutdown must preserve separately owned audio.
    const std::int16_t samples[] = {1, 2, 3, 4};
    const bool survives = window.audio.get_open() && window.audio.queue(samples);
    window.audio.close();
    if (!survives) std::cerr << "Audio did not survive GUI teardown.\n";
    return window.failed || !survives;
}
