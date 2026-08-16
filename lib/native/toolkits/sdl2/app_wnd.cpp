//
// Implements the SDL2 application-window backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>
#include <native/app_wnd.h>
#include <bindings.h>
#include <SDL2/SDL.h>
#ifdef HAVE_SDL2_TTF
#include <SDL2/SDL_ttf.h>
#endif
#include <stdexcept>
#include <algorithm>

#include "globals.h"

namespace native
{
    void app_wnd::apply_title() {
        SDL_Window *window =
            linux::sdl2::wnd_bindings.handle_from_object(this);
        if (!window)
            throw std::runtime_error(
                "SDL2: Missing SDL_Window binding for app_wnd.");

        SDL_SetWindowTitle(window, _title.c_str());
    }

    void app_wnd::create() const {
        if (_created)
            return;

        validate_owner_created();
        const bool initialize_runtime = linux::sdl2::windows.empty();
        if (initialize_runtime &&
            SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
            throw std::runtime_error(
                std::string(
                    "SDL2: Failed to initialize video subsystem: ") +
                SDL_GetError());

#ifdef HAVE_SDL2_TTF
        if (initialize_runtime && TTF_Init() != 0) {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);
            throw std::runtime_error(
                std::string("SDL2: Failed to initialize TTF: ") +
                TTF_GetError());
        }
#endif

        const bool use_default_position =
            (_bounds.p.x == 100 && _bounds.p.y == 100);
        const int window_x =
            use_default_position ? SDL_WINDOWPOS_CENTERED : _bounds.p.x;
        const int window_y =
            use_default_position ? SDL_WINDOWPOS_CENTERED : _bounds.p.y;

        SDL_Window *window = SDL_CreateWindow(_title.c_str(),
                                              window_x,
                                              window_y,
                                              _bounds.d.w,
                                              _bounds.d.h,
                                              SDL_WINDOW_HIDDEN);

        if (!window) {
#ifdef HAVE_SDL2_TTF
            if (initialize_runtime)
                TTF_Quit();
#endif
            const std::string error = SDL_GetError();
            if (initialize_runtime)
                SDL_QuitSubSystem(SDL_INIT_VIDEO);
            throw std::runtime_error("SDL2: Failed to create window: " +
                                     error);
        }

        if (!linux::sdl2::main_window)
            linux::sdl2::main_window = window;

        linux::sdl2::wnd_bindings.register_pair(
            window, const_cast<app_wnd *>(this));
        linux::sdl2::windows.push_back(
            const_cast<app_wnd *>(this));

#if SDL_VERSION_ATLEAST(2, 0, 5)
        if (get_modal()) {
            app_wnd *owner = get_owner();
            SDL_Window *owner_window =
                owner ? linux::sdl2::wnd_bindings
                            .handle_from_object(owner)
                      : nullptr;
            if (owner_window)
                SDL_SetWindowModalFor(window, owner_window);
        }
#endif

        _created = true;

        const_cast<app_wnd *>(this)->menu.attach(
            *const_cast<app_wnd *>(this));
        const_cast<app_wnd *>(this)->on_wnd_create.emit();
    }

    void app_wnd::show() const {
        if (!_created)
            throw std::runtime_error(
                "SDL2: Cannot show window before it is created.");

        SDL_Window *window =
            linux::sdl2::wnd_bindings.handle_from_object(
                const_cast<app_wnd *>(this));
        if (!window)
            throw std::runtime_error(
                "SDL2: Missing SDL_Window binding for app_wnd.");

        SDL_ShowWindow(window);
        if (get_modal()) {
            SDL_RaiseWindow(window);
#if SDL_VERSION_ATLEAST(2, 0, 5)
            SDL_SetWindowInputFocus(window);
#endif
        }
        invalidate();
    }

    void app_wnd::destroy() const {
        if (!_created)
            return;

        app_wnd *self = const_cast<app_wnd *>(this);
        SDL_Window *window =
            linux::sdl2::wnd_bindings.handle_from_object(self);
        app_wnd *owner = get_owner();
        self->on_native_destroy();
        if (window) {
            SDL_DestroyWindow(window);
            linux::sdl2::wnd_bindings.unregister_by_object(self);
        }
        if (linux::sdl2::main_window == window)
            linux::sdl2::main_window = nullptr;
        linux::sdl2::windows.erase(
            std::remove(linux::sdl2::windows.begin(),
                        linux::sdl2::windows.end(),
                        self),
            linux::sdl2::windows.end());

        if (get_modal() && owner) {
            app_wnd *focus = owner->get_input_enabled()
                                 ? owner
                                 : owner->get_active_modal();
            SDL_Window *focus_window =
                focus ? linux::sdl2::wnd_bindings
                            .handle_from_object(focus)
                      : nullptr;
            if (focus_window)
                SDL_RaiseWindow(focus_window);
#if SDL_VERSION_ATLEAST(2, 0, 5)
            if (focus_window)
                SDL_SetWindowInputFocus(focus_window);
#endif
        }

        if (!linux::sdl2::windows.empty())
            return;
#ifdef HAVE_SDL2_TTF
        TTF_Quit();
#endif
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

} // namespace native
