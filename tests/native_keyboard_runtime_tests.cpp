//
// Tests real SDL event dispatch and audio ownership across GUI-loop teardown.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>
#include "toolkits/sdl2/globals.h"
#include <SDL.h>
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
        native::audio_out audio{44100, 1, 22050};
        bool failed = false;
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
                    destroy();
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
