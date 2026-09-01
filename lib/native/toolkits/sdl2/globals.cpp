//
// Implements the SDL2 shared backend-state backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <SDL2/SDL.h>

#include <native.h>
#include <bindings.h>

#include "globals.h"

namespace linux::sdl2
{
    int content_origin_y(native::wnd *window) {
        auto *application_window =
            dynamic_cast<native::app_wnd *>(window);
        return application_window && application_window->menu.id()
                   ? menu_bar_height
                   : 0;
    }

    SDL_Window *main_window = nullptr;
    native::bindings<SDL_Window *, native::wnd *> wnd_bindings;
    native::bindings<native::wnd *, sdl2_gpx *> wnd_gpx_bindings;
    native::bindings<uint32_t, sdl2_menu *> menu_bindings;
    native::bindings<native::button *, sdl2_button *> button_bindings;
    native::bindings<native::check *, sdl2_check *> check_bindings;
    native::bindings<native::radio *, sdl2_radio *> radio_bindings;
    native::bindings<native::list *, sdl2_list *> list_bindings;
    native::bindings<native::text_edit *, sdl2_text_edit *>
        text_edit_bindings;
    native::bindings<native::accordion *, sdl2_collection *>
        accordion_bindings;
    native::bindings<native::icon_view *, sdl2_collection *>
        icon_view_bindings;
    native::bindings<native::tree_view *, sdl2_collection *>
        tree_view_bindings;
    native::bindings<native::table_view *, sdl2_collection *>
        table_view_bindings;
    native::bindings<native::code_edit *, sdl2_collection *>
        code_edit_bindings;
    std::vector<native::check *> checks;
    std::vector<native::radio *> radios;
    std::vector<native::list *> lists;
    std::vector<native::button *> buttons;
    std::vector<native::text_edit *> text_edits;
    std::vector<native::accordion *> accordions;
    std::vector<native::icon_view *> icon_views;
    std::vector<native::tree_view *> tree_views;
    std::vector<native::table_view *> table_views;
    std::vector<native::code_edit *> code_edits;
    std::vector<native::app_wnd *> windows;
#ifdef HAVE_SDL2_TTF
    native::bindings<uint32_t, sdl2_font *> font_bindings;
#endif
} // namespace linux::sdl2
