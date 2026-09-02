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
    namespace x11_clipboard
    {
        // Serve pending foreign X11 clipboard selection requests.
        void service();

        // Release the foreign X11 clipboard connection.
        void shutdown();
    } // namespace x11_clipboard

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

    struct sdl2_menu
    {
        native::app_wnd *owner = nullptr;
        struct top_entry
        {
            std::string title;
            std::vector<native::main_menu::menu_entry> items;
            int x0 = 0, x1 = 0;
        };
        std::vector<top_entry> tops;
        int open_idx = -1;
        int hover_top = -1;
        int hover_item = -1;
        int popup_x = 0;
        int popup_y = 0;
    };

    struct sdl2_button
    {
        native::button *owner = nullptr;
        native::wnd *parent = nullptr;
        native::rect bounds = {};
        std::string label;
        bool hover = false;
        bool pressed = false;
        bool visible = false;
    };

    struct sdl2_check
    {
        native::wnd *parent = nullptr;
        native::rect bounds = {};
        std::string label;
        bool checked = false;
        bool hover = false;
        bool pressed = false;
        bool visible = false;
    };

    struct sdl2_radio
    {
        native::wnd *parent = nullptr;
        native::rect bounds = {};
        std::string label;
        bool selected = false;
        bool hover = false;
        bool pressed = false;
        bool visible = false;
    };

    struct sdl2_list
    {
        native::wnd *parent = nullptr;
        native::rect bounds = {};
        std::vector<std::string> items;
        int selected_index = -1;
        bool visible = false;
    };
    struct sdl2_combo_box
    {
        native::wnd *parent = nullptr;
        bool visible = false;
        bool open = false;
        bool focused = false;
    };

    struct sdl2_text_edit
    {
        native::wnd *parent = nullptr;
        native::rect bounds = {};
        std::size_t cursor = 0;
        std::size_t anchor = 0;
        std::size_t first_line = 0;
        int horizontal_scroll = 0;
        bool visible = false;
        bool focused = false;
        bool mouse_selecting = false;
    };

    struct sdl2_collection
    {
        bool visible = false;
    };

    // Render an emulated menu bar and open popup.
    void render_menu(sdl2_menu *menu,
                     native::gpx &graphics,
                     int window_width,
                     int window_height);

    // Handle a menu click, returning whether the menu consumed it.
    bool handle_menu_click(sdl2_menu *m, int x, int y, int win_w);

    // Update menu hover state from pointer motion.
    bool handle_menu_motion(sdl2_menu *m, int x, int y, int win_w);

    // Activate a portable accelerator from an SDL key transition.
    bool handle_menu_key(sdl2_menu *menu,
                         const SDL_KeyboardEvent &event);

    // Update emulated buttons from a mouse-button transition.
    bool handle_button_mouse(
        native::wnd *owner, int x, int y, bool pressed, bool released);

    // Update emulated button hover state from pointer motion.
    bool handle_button_motion(native::wnd *owner, int x, int y);

    // Render every visible emulated button owned by a window.
    void render_buttons(native::wnd *owner, native::gpx &g);

    // Update emulated checks from a mouse-button transition.
    bool handle_check_mouse(native::wnd *, int, int, bool, bool);

    // Update emulated check hover state from pointer motion.
    bool handle_check_motion(native::wnd *, int, int);

    // Render every visible emulated check owned by a window.
    void render_checks(native::wnd *, native::gpx &);

    // Update emulated radios from a mouse-button transition.
    bool handle_radio_mouse(native::wnd *, int, int, bool, bool);

    // Update emulated radio hover state from pointer motion.
    bool handle_radio_motion(native::wnd *, int, int);

    // Render every visible emulated radio owned by a window.
    void render_radios(native::wnd *, native::gpx &);

    // Update an emulated list from a mouse-button transition.
    bool handle_list_mouse(native::wnd *, int, int, bool);

    // Render every visible emulated list owned by a window.
    void render_lists(native::wnd *, native::gpx &);

    bool handle_combo_mouse(native::wnd *, int, int, bool);
    bool handle_combo_key(native::wnd *, const SDL_KeyboardEvent &);
    bool handle_combo_text(native::wnd *, const char *);
    void render_combo_boxes(native::wnd *, native::gpx &);

    // Focus or position an emulated editor from a mouse press.
    bool handle_text_edit_mouse(native::wnd *, int, int, bool);

    // Extend an active pointer selection in an emulated editor.
    bool handle_text_edit_motion(native::wnd *, int, int);

    // Apply navigation or an editing shortcut to the focused editor.
    bool handle_text_edit_key(native::wnd *, const SDL_KeyboardEvent &);

    // Insert one SDL UTF-8 text-input payload into the focused editor.
    bool handle_text_edit_input(native::wnd *, const char *);

    // Render every visible emulated editor owned by a window.
    void render_text_edits(native::wnd *, native::gpx &);

    void render_collections(native::wnd *, native::gpx &);
    void render_tab_views(native::wnd *, native::gpx &);
    bool handle_collection_mouse(native::wnd *,
                                 int,
                                 int,
                                 bool,
                                 bool,
                                 int);
    bool handle_collection_wheel(native::wnd *, int, int, int);
    bool handle_collection_motion(native::wnd *, int, int);
    bool handle_collection_key(native::wnd *, const SDL_KeyboardEvent &);
    bool handle_collection_text(native::wnd *, const char *);

    //
    // Return the client area's vertical origin inside its window.
    //
    // Notes:
    //      An emulated menu bar is painted in window coordinates
    //      above the client area, so a window carrying one is taller
    //      than the client it exposes. Geometry crossing the public
    //      boundary converts through this in both directions.
    //
    int content_origin_y(native::wnd *window);

    // Return the rendered width of text in the active control font.
    int text_width(const std::string &text);

    // Return the active control-font height.
    int text_height();

    // Draw text through SDL_ttf or the built-in bitmap fallback.
    void draw_text(SDL_Renderer *renderer,
                   const std::string &text,
                   int x,
                   int y,
                   SDL_Color color);

    extern SDL_Window *main_window;
    extern native::bindings<SDL_Window *, native::wnd *> wnd_bindings;
    extern native::bindings<native::wnd *, sdl2_gpx *> wnd_gpx_bindings;
    extern native::bindings<uint32_t, sdl2_menu *> menu_bindings;
    extern native::bindings<native::button *, sdl2_button *>
        button_bindings;
    extern native::bindings<native::check *, sdl2_check *>
        check_bindings;
    extern native::bindings<native::radio *, sdl2_radio *>
        radio_bindings;
    extern native::bindings<native::list *, sdl2_list *> list_bindings;
    extern native::bindings<native::combo_box *, sdl2_combo_box *>
        combo_box_bindings;
    extern native::bindings<native::text_edit *, sdl2_text_edit *>
        text_edit_bindings;
    extern native::bindings<native::accordion *, sdl2_collection *>
        accordion_bindings;
    extern native::bindings<native::tab_view *, sdl2_collection *>
        tab_view_bindings;
    extern native::bindings<native::split_view *, sdl2_collection *>
        split_view_bindings;
    extern native::bindings<native::icon_view *, sdl2_collection *>
        icon_view_bindings;
    extern native::bindings<native::tree_view *, sdl2_collection *>
        tree_view_bindings;
    extern native::bindings<native::table_view *, sdl2_collection *>
        table_view_bindings;
    extern native::bindings<native::code_edit *, sdl2_collection *>
        code_edit_bindings;
    extern std::vector<native::check *> checks;
    extern std::vector<native::radio *> radios;
    extern std::vector<native::list *> lists;
    extern std::vector<native::combo_box *> combo_boxes;
    extern std::vector<native::button *> buttons;
    extern std::vector<native::text_edit *> text_edits;
    extern std::vector<native::accordion *> accordions;
    extern std::vector<native::tab_view *> tab_views;
    extern std::vector<native::icon_view *> icon_views;
    extern std::vector<native::tree_view *> tree_views;
    extern std::vector<native::table_view *> table_views;
    extern std::vector<native::code_edit *> code_edits;
    extern std::vector<native::app_wnd *> windows;
#ifdef HAVE_SDL2_TTF
    extern native::bindings<uint32_t, sdl2_font *> font_bindings;
#endif
} // namespace linux::sdl2
