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
    SDL_Window *main_window = nullptr;
    native::bindings<SDL_Window *, native::wnd *> wnd_bindings;
    native::bindings<native::wnd *, sdl2_gpx *> wnd_gpx_bindings;
    native::bindings<uint32_t, sdl2_menu *> menu_bindings;
    native::bindings<native::button *, sdl2_button *> button_bindings;
    native::bindings<native::check *, sdl2_check *> check_bindings;
    native::bindings<native::radio *, sdl2_radio *> radio_bindings;
    native::bindings<native::list *, sdl2_list *> list_bindings;
    std::vector<native::check *> checks;
    std::vector<native::radio *> radios;
    std::vector<native::list *> lists;
    std::vector<native::button *> buttons;
    std::vector<native::app_wnd *> windows;
#ifdef HAVE_SDL2_TTF
    native::bindings<uint32_t, sdl2_font *> font_bindings;
#endif
} // namespace linux::sdl2
