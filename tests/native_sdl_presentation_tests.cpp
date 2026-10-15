//
// Checks the real SDL renderer's presentation policy. UI delivery and input
// share this thread, so the backend must not request a vertical-refresh wait.
// Run with the desktop video driver to exercise accelerated presentation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include "../lib/native/toolkits/sdl2/globals.h"
#include <iostream>

namespace {
class presentation_window final : public native::app_wnd {
public:
    // Inspect the renderer after the initial frame and event-loop startup.
    presentation_window() : native::app_wnd("SDL presentation", 20, 20, 320, 200) {
        on_wnd_create.connect([this] {
            native::app::post([this] { verify(); });
            return false;
        });
    }
    int result = 0;

private:
    // SDL software reports VSYNC capability even without a display. Only
    // accelerated renderer flags report the requested presentation policy.
    void verify() {
        auto *state = linux::sdl2::wnd_gpx_bindings.object_from_handle(this);
        SDL_RendererInfo renderer{};
        if (!state || !state->renderer ||
            SDL_GetRendererInfo(state->renderer, &renderer) != 0 ||
            ((renderer.flags & SDL_RENDERER_ACCELERATED) &&
             (renderer.flags & SDL_RENDERER_PRESENTVSYNC))) {
            std::cerr << "FAILED: SDL presentation requests a refresh wait\n";
            result = 1;
        } else {
            std::cout << ((renderer.flags & SDL_RENDERER_ACCELERATED)
                ? "PASS nonblocking accelerated SDL presentation: "
                : "PASS software SDL startup (hardware policy not exercised): ")
                      << renderer.name << '\n';
        }
        request_close();
    }
};
}

// Native owns platform startup and renderer/window shutdown.
int program(int, char **) {
    presentation_window window;
    const int status = native::app::run(window);
    return status ? status : window.result;
}
