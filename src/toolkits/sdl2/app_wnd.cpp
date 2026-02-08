#include <native.h>
#include <bindings.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>

#include "globals.h"

namespace sdl
{
    SDL_Window *main_window = nullptr;
    extern native::bindings<SDL_Window *, native::wnd *> wnd_bindings;
    extern native::bindings<native::wnd *, sdl2gpx *> wnd_gpx_bindings;
}

namespace native
{
    void app_wnd::create() const
    {
        // Initialize SDL video subsystem
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
        {
            std::cerr << "SDL2: Failed to initialize video subsystem: "
                      << SDL_GetError() << std::endl;
            return;
        }

        // Initialize SDL_ttf
        if (TTF_Init() != 0)
        {
            std::cerr << "SDL2: Failed to initialize TTF: "
                      << TTF_GetError() << std::endl;
        }

        SDL_Window *window = SDL_CreateWindow(
            title().c_str(),
            bounds().p.x, bounds().p.y,
            bounds().d.w, bounds().d.h,
            SDL_WINDOW_SHOWN);

        if (!window)
        {
            std::cerr << "SDL2: Failed to create window: "
                      << SDL_GetError() << std::endl;
            return;
        }

        // Store the main window pointer
        sdl::main_window = window;

        // Register in bindings
        sdl::wnd_bindings.register_pair(window, const_cast<app_wnd *>(this));

        // Mark as created
        _created = true;

        // Emit create signal
        on_wnd_create.emit();
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
    }

    void app_wnd::destroy() const
    {
        if (!_created)
            return;

        app_wnd *self = const_cast<app_wnd *>(this);

        // Clean up graphics cache
        if (auto *cache = sdl::wnd_gpx_bindings.from_a(self))
        {
            if (cache->renderer)
                SDL_DestroyRenderer(cache->renderer);
            if (cache->font)
                TTF_CloseFont(cache->font);
            delete cache;
            sdl::wnd_gpx_bindings.unregister_by_a(self);
        }

        // Destroy window
        SDL_Window *window = sdl::wnd_bindings.from_b(self);
        if (window)
        {
            SDL_DestroyWindow(window);
            sdl::wnd_bindings.unregister_by_b(self);
        }

        _created = false;

        // Quit SDL_ttf and SDL
        TTF_Quit();
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
}
