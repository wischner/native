//
// Implements the SDL2 display-detection backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>
#include <string>
#include <utility>

#include <SDL2/SDL.h>

#include <native.h>

namespace native
{
    const std::vector<screen> &screen::detect() {
        _screens.clear();

        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
            throw std::runtime_error(
                std::string(
                    "SDL2: Failed to initialize video: ") +
                SDL_GetError());
        }

        int display_count = SDL_GetNumVideoDisplays();
        if (display_count < 0)
            throw std::runtime_error(
                std::string("SDL2: Failed to enumerate displays: ") +
                SDL_GetError());

        std::vector<screen> detected;

        for (int i = 0; i < display_count; ++i) {
            SDL_Rect bounds;
            SDL_Rect usable;
            if (SDL_GetDisplayBounds(i, &bounds) != 0 ||
                SDL_GetDisplayUsableBounds(i, &usable) != 0) {
                throw std::runtime_error(
                    std::string(
                        "SDL2: Failed to query display geometry: ") +
                    SDL_GetError());
            }

            rect screen_bounds(bounds.x, bounds.y, bounds.w, bounds.h);
            rect work_area(usable.x, usable.y, usable.w, usable.h);

            bool is_primary = (i == 0);

            detected.emplace_back(
                i,
                screen_bounds,
                work_area,
                is_primary);
        }

        _screens = std::move(detected);
        normalize();
        return _screens;
    }

} // namespace native
