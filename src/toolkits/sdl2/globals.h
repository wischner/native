#pragma once

#include <SDL2/SDL.h>
#ifdef HAVE_SDL2_TTF
#include <SDL2/SDL_ttf.h>
#endif

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
        bool invalidated = true;

        // Cached font
        std::string font_name;
#ifdef HAVE_SDL2_TTF
        TTF_Font *font = nullptr;
#endif
    } sdl2gpx;

    extern SDL_Window *main_window;
    extern native::bindings<SDL_Window *, native::wnd *> wnd_bindings;
    extern native::bindings<native::wnd *, sdl2gpx *> wnd_gpx_bindings;
}
