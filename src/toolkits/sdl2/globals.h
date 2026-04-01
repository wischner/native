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
        int hover_top = -1;
        int hover_item = -1;
        int popup_x  = 0;
        int popup_y  = 0;
    };

    struct sdl2button {
        native::button *owner = nullptr;
        native::wnd *parent = nullptr;
        native::rect bounds = {};
        std::string label;
        bool hover = false;
        bool pressed = false;
        bool visible = false;
    };

    void render_menu(sdl2menu *m, native::gpx &g, int win_w, int win_h);
    // Returns true if the click was consumed by the menu.
    bool handle_menu_click(sdl2menu *m, int x, int y, int win_w);
    bool handle_menu_motion(sdl2menu *m, int x, int y, int win_w);
    bool handle_button_mouse(native::wnd *owner, int x, int y, bool pressed, bool released);
    bool handle_button_motion(native::wnd *owner, int x, int y);
    void render_buttons(native::wnd *owner, native::gpx &g);
    int text_width(const std::string &text);
    int text_height();
    void draw_text(SDL_Renderer *r, const std::string &text, int x, int y, SDL_Color col);

    extern SDL_Window *main_window;
    extern native::bindings<SDL_Window *, native::wnd *> wnd_bindings;
    extern native::bindings<native::wnd *, sdl2gpx *> wnd_gpx_bindings;
    extern native::bindings<uint32_t, sdl2menu *> menu_bindings;
    extern native::bindings<native::button *, sdl2button *> button_bindings;
#ifdef HAVE_SDL2_TTF
    extern native::bindings<uint32_t, sdl2font *> font_bindings;
#endif
}
