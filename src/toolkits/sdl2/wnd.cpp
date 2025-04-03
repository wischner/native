#include <native.h>
#include <bindings.h>
#include <SDL2/SDL.h>

namespace sdl
{
    native::bindings<SDL_Window *, native::wnd *> wnd_bindings;
}