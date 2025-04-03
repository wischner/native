#include <native.h>
#include "bindings.h"
#include <SDL2/SDL.h>
#include <iostream>

namespace sdl
{
    extern SDL_Window *main_window;
    extern native::bindings<SDL_Window *, native::wnd *> wnd_bindings;
}

namespace native
{
    int app::main_loop()
    {
        SDL_Event event;
        bool running = true;

        while (running)
        {
            while (SDL_PollEvent(&event))
            {
                native::wnd *wnd = sdl::wnd_bindings.from_a(
                    event.window.windowID ? SDL_GetWindowFromID(event.window.windowID) : sdl::main_window);

                if (!wnd)
                    continue;

                switch (event.type)
                {
                case SDL_QUIT:
                    running = false;
                    break;

                case SDL_MOUSEMOTION:
                    wnd->on_mouse_move.emit(point(event.motion.x, event.motion.y));
                    break;

                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP:
                {
                    mouse_button btn = mouse_button::none;
                    switch (event.button.button)
                    {
                    case SDL_BUTTON_LEFT:
                        btn = mouse_button::left;
                        break;
                    case SDL_BUTTON_RIGHT:
                        btn = mouse_button::right;
                        break;
                    case SDL_BUTTON_MIDDLE:
                        btn = mouse_button::middle;
                        break;
                    }

                    if (btn != mouse_button::none)
                    {
                        mouse_event me(btn, point(event.button.x, event.button.y));
                        wnd->on_mouse_click.emit(me);
                    }
                    break;
                }

                case SDL_MOUSEWHEEL:
                {
                    wheel_direction dir = event.wheel.x != 0
                                              ? wheel_direction::horizontal
                                              : wheel_direction::vertical;

                    coord delta = (dir == wheel_direction::horizontal)
                                      ? event.wheel.x
                                      : event.wheel.y;

                    mouse_wheel_event whe(
                        point(), // SDL doesn't always give x,y here
                        delta,
                        dir);

                    wnd->on_mouse_wheel.emit(whe);
                    break;
                }

                case SDL_WINDOWEVENT:
                    switch (event.window.event)
                    {
                    case SDL_WINDOWEVENT_RESIZED:
                        wnd->on_wnd_resize.emit(size(
                            event.window.data1,
                            event.window.data2));
                        break;

                    case SDL_WINDOWEVENT_MOVED:
                        wnd->on_wnd_move.emit(point(
                            event.window.data1,
                            event.window.data2));
                        break;
                    }
                    break;

                default:
                    break;
                }
            }

            // Idle / frame delay
            SDL_Delay(16);
        }

        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return 0;
    }
}
