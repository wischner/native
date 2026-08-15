//
// Declares internal SDL2 shared backend-state types and state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <SDL2/SDL.h>
#ifdef HAVE_SDL2_TTF
#include <SDL2/SDL_ttf.h>
#endif

#include <native.h>
#include <bindings.h>

namespace linux::sdl2
{
    // SDL events carry handles, so process-wide registries
    // recover the corresponding C++ objects during event dispatch.
#ifdef HAVE_SDL2_TTF
    // Platform handle for a font_t — owns a TTF_Font.
    struct sdl2_font
    {
        TTF_Font *ttf_font;
    };
#endif

    // Graphics cache structure for SDL2
    struct sdl2_gpx
    {
        SDL_Renderer *renderer = nullptr; // Cached renderer per window

        // Cached draw parameters
        native::rgba current_fg = 0xFFFFFFFF;
        int current_thickness = -1;

        // Clip region
        native::rect clip = {};
        bool dirty_clip = true;
        bool invalidated = true;
    };

    static constexpr int menu_bar_height = 24;

    struct sdl2_menu {
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

    struct sdl2_button {
        native::button *owner = nullptr;
        native::wnd *parent = nullptr;
        native::rect bounds = {};
        std::string label;
        bool hover = false;
        bool pressed = false;
        bool visible = false;
    };

    // Render an emulated menu bar and open popup.
    void render_menu(
        sdl2_menu *menu,
        native::gpx &graphics,
        int window_width,
        int window_height);

    // Handle a menu click, returning whether the menu consumed it.
    bool handle_menu_click(sdl2_menu *m, int x, int y, int win_w);

    // Update menu hover state from pointer motion.
    bool handle_menu_motion(sdl2_menu *m, int x, int y, int win_w);

    // Update emulated buttons from a mouse-button transition.
    bool handle_button_mouse(
        native::wnd *owner,
        int x,
        int y,
        bool pressed,
        bool released);

    // Update emulated button hover state from pointer motion.
    bool handle_button_motion(native::wnd *owner, int x, int y);

    // Render every visible emulated button owned by a window.
    void render_buttons(native::wnd *owner, native::gpx &g);

    // Return the rendered width of text in the active control font.
    int text_width(const std::string &text);

    // Return the active control-font height.
    int text_height();

    // Draw text through SDL_ttf or the built-in bitmap fallback.
    void draw_text(
        SDL_Renderer *renderer,
        const std::string &text,
        int x,
        int y,
        SDL_Color color);

    extern SDL_Window *main_window;
    extern native::bindings<SDL_Window *, native::wnd *> wnd_bindings;
    extern native::bindings<native::wnd *, sdl2_gpx *> wnd_gpx_bindings;
    extern native::bindings<uint32_t, sdl2_menu *> menu_bindings;
    extern native::bindings<
        native::button *,
        sdl2_button *> button_bindings;
#ifdef HAVE_SDL2_TTF
    extern native::bindings<uint32_t, sdl2_font *> font_bindings;
#endif
}
