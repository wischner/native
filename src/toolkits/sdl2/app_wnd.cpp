#include <native.h>
#include <bindings.h>
#include <SDL2/SDL.h>
#include <iostream>

namespace sdl
{
    SDL_Window *main_window = nullptr;
    extern native::bindings<SDL_Window *, native::wnd *> wnd_bindings;
}

namespace native
{
    void app_wnd::create() const
    {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
        {
            std::cerr << "SDL2: Failed to initialize video subsystem: "
                      << SDL_GetError() << std::endl;
            return;
        }

        SDL_Window *window = SDL_CreateWindow(
            title().c_str(),
            x(), y(),
            width(), height(),
            SDL_WINDOW_SHOWN);

        if (!window)
        {
            std::cerr << "SDL2: Failed to create window: "
                      << SDL_GetError() << std::endl;
            return;
        }

        // Store the main window pointer (optional)
        sdl::main_window = window;

        // Register in bindings
        sdl::wnd_bindings.register_pair(window, const_cast<app_wnd *>(this));
    }

    void app_wnd::show() const
    {
        SDL_Window *window = sdl::wnd_bindings.from_b(const_cast<app_wnd *>(this));
        if (!window)
        {
            std::cerr << "SDL2: Can't show window, not created." << std::endl;
            return;
        }

        SDL_ShowWindow(window);

        SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (renderer)
        {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255); // white background
            SDL_RenderClear(renderer);
            SDL_RenderPresent(renderer);
        }
        else
        {
            std::cerr << "SDL2: Failed to create renderer: " << SDL_GetError() << std::endl;
        }
    }
}
