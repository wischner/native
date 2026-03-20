#include <native.h>
#include <bindings.h>
#include <SDL2/SDL.h>
#ifdef HAVE_SDL2_TTF
#include <SDL2/SDL_ttf.h>
#endif
#include <iostream>

#include "globals.h"

namespace native
{
    app_wnd::app_wnd(std::string title, coord x, coord y, dim w, dim h)
        : wnd(x, y, w, h), _title(std::move(title))
    {
    }

    app_wnd &app_wnd::set_title(const std::string &title)
    {
        _title = title;

        if (_created)
        {
            SDL_Window *window = sdl::wnd_bindings.from_b(this);
            if (window)
                SDL_SetWindowTitle(window, _title.c_str());
        }

        return *this;
    }

    const std::string &app_wnd::title() const
    {
        return _title;
    }

    void app_wnd::create() const
    {
        if (_created)
            return;

        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
        {
            std::cerr << "SDL2: Failed to initialize video subsystem: "
                      << SDL_GetError() << std::endl;
            return;
        }

#ifdef HAVE_SDL2_TTF
        if (TTF_Init() != 0)
        {
            std::cerr << "SDL2: Failed to initialize TTF: "
                      << TTF_GetError() << std::endl;
        }
#endif

        const bool use_default_position = (_bounds.p.x == 100 && _bounds.p.y == 100);
        const int window_x = use_default_position ? SDL_WINDOWPOS_CENTERED : _bounds.p.x;
        const int window_y = use_default_position ? SDL_WINDOWPOS_CENTERED : _bounds.p.y;

        SDL_Window *window = SDL_CreateWindow(
            _title.c_str(),
            window_x, window_y,
            _bounds.d.w, _bounds.d.h,
            SDL_WINDOW_SHOWN);

        if (!window)
        {
            std::cerr << "SDL2: Failed to create window: "
                      << SDL_GetError() << std::endl;
            return;
        }

        sdl::main_window = window;

        sdl::wnd_bindings.register_pair(window, const_cast<app_wnd *>(this));

        _created = true;

        const_cast<app_wnd *>(this)->on_wnd_create.emit();
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
        invalidate();
    }

    void app_wnd::destroy() const
    {
        if (!_created)
            return;

        app_wnd *self = const_cast<app_wnd *>(this);

        if (auto *cache = sdl::wnd_gpx_bindings.from_a(self))
        {
            if (cache->renderer)
                SDL_DestroyRenderer(cache->renderer);
#ifdef HAVE_SDL2_TTF
            if (cache->font)
                TTF_CloseFont(cache->font);
#endif
            delete cache;
            sdl::wnd_gpx_bindings.unregister_by_a(self);
        }

        SDL_Window *window = sdl::wnd_bindings.from_b(self);
        if (window)
        {
            SDL_DestroyWindow(window);
            sdl::wnd_bindings.unregister_by_b(self);
        }

        _created = false;

#ifdef HAVE_SDL2_TTF
        TTF_Quit();
#endif
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

} // namespace native
