#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <native.h>
#include <bindings.h>

namespace sdl
{
    // Graphics cache structure for SDL2
    typedef struct
    {
        SDL_Renderer *renderer = nullptr; // Cached renderer per window

        // Cached draw parameters
        native::rgba current_fg = 0xFFFFFFFF;
        int current_thickness = -1;

        // Clip region
        native::rect clip = {};
        bool dirty_clip = true;

        // Cached font
        std::string font_name;
        TTF_Font *font = nullptr;
    } sdl2gpx;

    extern native::bindings<SDL_Window *, native::wnd *> wnd_bindings;
    extern native::bindings<native::wnd *, sdl2gpx *> wnd_gpx_bindings;
}
