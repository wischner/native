#include <SDL2/SDL.h>

#include <native.h>
#include <bindings.h>

#include "globals.h"

namespace sdl
{
    SDL_Window *main_window = nullptr;
    native::bindings<SDL_Window *, native::wnd *> wnd_bindings;
    native::bindings<native::wnd *, sdl2gpx *> wnd_gpx_bindings;
    native::bindings<uint32_t, sdl2menu *> menu_bindings;
    native::bindings<native::button *, sdl2button *> button_bindings;
#ifdef HAVE_SDL2_TTF
    native::bindings<uint32_t, sdl2font *> font_bindings;
#endif
}
