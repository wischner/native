#include "native.h"
#include <SDL2/SDL.h>
#include <iostream>

namespace native
{

    static mouse_button map_sdl_button(uint8_t sdl_button)
    {
        switch (sdl_button)
        {
        case SDL_BUTTON_LEFT:
            return mouse_button::left;
        case SDL_BUTTON_RIGHT:
            return mouse_button::right;
        case SDL_BUTTON_MIDDLE:
            return mouse_button::middle;
        case SDL_BUTTON_X1:
            return mouse_button::x1;
        case SDL_BUTTON_X2:
            return mouse_button::x2;
        default:
            return mouse_button::none;
        }
    }

    int app::main_loop()
    {
        SDL_Event event;
        bool running = true;

        // Optional: emit create if not already handled in show()
        if (auto *wnd = app::main_wnd())
            wnd->on_wnd_create.emit();

        while (running)
        {
            while (SDL_PollEvent(&event))
            {
                auto *wnd = app::main_wnd();
                if (!wnd)
                    continue;

                switch (event.type)
                {
                case SDL_QUIT:
                case SDL_KEYDOWN:
                    running = false;
                    break;

                case SDL_WINDOWEVENT:
                    switch (event.window.event)
                    {
                    case SDL_WINDOWEVENT_MOVED:
                        wnd->on_wnd_move.emit({static_cast<coord>(event.window.data1),
                                               static_cast<coord>(event.window.data2)});
                        break;

                    case SDL_WINDOWEVENT_RESIZED:
                        wnd->on_wnd_resize.emit({static_cast<coord>(event.window.data1),
                                                 static_cast<coord>(event.window.data2)});
                        break;
                    }
                    break;

                case SDL_MOUSEMOTION:
                    wnd->on_mouse_move.emit({static_cast<coord>(event.motion.x),
                                             static_cast<coord>(event.motion.y)});
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    wnd->on_mouse_click.emit(mouse_event{
                        map_sdl_button(event.button.button),
                        point(static_cast<coord>(event.button.x),
                              static_cast<coord>(event.button.y))});
                    break;

                case SDL_MOUSEWHEEL:
                {
                    int mx, my;
                    SDL_GetMouseState(&mx, &my);
                    wnd->on_mouse_wheel.emit(mouse_wheel_event{
                        point(static_cast<coord>(mx), static_cast<coord>(my)),
                        static_cast<coord>(event.wheel.y != 0 ? event.wheel.y : event.wheel.x),
                        (event.wheel.y != 0)
                            ? wheel_direction::vertical
                            : wheel_direction::horizontal});
                    break;
                }
                }
            }

            SDL_Delay(16); // idle / simulate frame
        }

        SDL_Quit();
        return 0;
    }

} // namespace native
