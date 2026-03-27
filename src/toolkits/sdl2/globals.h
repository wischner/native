#pragma once

#include <SDL2/SDL.h>
#ifdef HAVE_SDL2_TTF
#include <SDL2/SDL_ttf.h>
#endif

#include <native.h>
#include <bindings.h>

namespace sdl
{
#ifdef HAVE_SDL2_TTF
    // Platform handle for a font_t — owns a TTF_Font.
    struct sdl2font
    {
        TTF_Font *ttf_font;
    };
#endif

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
    } sdl2gpx;

    static constexpr int MENU_BAR_H = 24;

    struct sdl2menu {
        native::app_wnd *owner = nullptr;
        struct top_entry {
            std::string title;
            std::vector<std::pair<int, std::string>> items;
            int x0 = 0, x1 = 0;
        };
        std::vector<top_entry> tops;
        int open_idx = -1;
        int popup_x  = 0;
        int popup_y  = 0;
    };

    void render_menu(sdl2menu *m, SDL_Renderer *r, int win_w, int win_h);
    // Returns true if the click was consumed by the menu.
    bool handle_menu_click(sdl2menu *m, int x, int y, SDL_Renderer *r, int win_w);

    extern SDL_Window *main_window;
    extern native::bindings<SDL_Window *, native::wnd *> wnd_bindings;
    extern native::bindings<native::wnd *, sdl2gpx *> wnd_gpx_bindings;
    extern native::bindings<uint32_t, sdl2menu *> menu_bindings;
#ifdef HAVE_SDL2_TTF
    extern native::bindings<uint32_t, sdl2font *> font_bindings;
#endif
}
