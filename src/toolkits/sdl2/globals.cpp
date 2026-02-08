#include <SDL2/SDL.h>

#include <native.h>
#include <bindings.h>

#include "globals.h"

namespace sdl
{
    native::bindings<SDL_Window *, native::wnd *> wnd_bindings;
    native::bindings<native::wnd *, sdl2gpx *> wnd_gpx_bindings;
}
