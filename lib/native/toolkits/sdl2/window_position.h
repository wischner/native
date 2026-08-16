//
// Declares SDL2 top-level window placement constrained to a usable
// display area while retaining access to native decorations.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <SDL2/SDL.h>

#include <native/geometry.h>

namespace linux::sdl2
{
    //
    // Constrain a preferred client position to a usable display area.
    // A null window uses conservative decoration dimensions.
    //
    native::point constrain_window_position(
        SDL_Window *window,
        const native::point &preferred,
        const native::size &dimensions);
} // namespace linux::sdl2
