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
#include <algorithm>
#include <cstdlib>
#include <stdexcept>

#include "globals.h"
#include "window_position.h"

#ifdef HAVE_SDL2_TTF
namespace
{
    bool ttf_initialized = false;

    // Shut SDL_ttf down after function-static font objects release
    // their
    // TTF_Font handles during normal process teardown.
    void shutdown_ttf() {
        if (!ttf_initialized)
            return;
        TTF_Quit();
        ttf_initialized = false;
    }
} // namespace
#endif

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
        if (!ttf_initialized) {
            if (TTF_Init() != 0) {
                SDL_QuitSubSystem(SDL_INIT_VIDEO);
                throw std::runtime_error(
                    std::string("SDL2: Failed to initialize TTF: ") +
                    TTF_GetError());
            }
            ttf_initialized = true;
            std::atexit(shutdown_ttf);
        }
#endif

        const point position =
            linux::sdl2::constrain_window_position(
                nullptr, _bounds.p, _bounds.d);

        // Resizable, like the Win32, Cocoa, Haiku, and X11 shells.
        // Without it a toolkit window can never change size, so a
        // layout would arrange its children exactly once and no
        // resize would ever be reported.
        SDL_Window *window =
            SDL_CreateWindow(_title.c_str(),
                             position.x,
                             position.y,
                             _bounds.d.w,
                             _bounds.d.h,
                             SDL_WINDOW_HIDDEN |
                                 SDL_WINDOW_RESIZABLE);

        if (!window) {
            const std::string error = SDL_GetError();
            if (initialize_runtime)
                SDL_QuitSubSystem(SDL_INIT_VIDEO);
            throw std::runtime_error("SDL2: Failed to create window: " +
                                     error);
        }

        if (!linux::sdl2::main_window)
            linux::sdl2::main_window = window;

        auto *self = const_cast<app_wnd *>(this);
        int actual_x = 0;
        int actual_y = 0;
        SDL_GetWindowPosition(window, &actual_x, &actual_y);
        self->on_native_move(point(actual_x, actual_y));

        linux::sdl2::wnd_bindings.register_pair(
            window, self);
        linux::sdl2::windows.push_back(self);

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

        self->menu.attach(*self);

        // An emulated menu bar sits above the client area, so the
        // portable client height is smaller than the window SDL2 just
        // created. Every backend that reports geometry through the
        // event loop delivers this on its own; SDL2 sends a resize
        // only when the user causes one, so report it here. Doing it
        // before on_wnd_create means user code that installs a layout
        // arranges children against the size SDL2 actually renders.
        const int menu_height = linux::sdl2::content_origin_y(self);

        // A menu bar has just claimed part of the window, so grow it
        // back by that much to leave the client the caller asked for,
        // and stop the user shrinking the window into the menu.
        if (menu_height > 0) {
            SDL_SetWindowSize(window,
                              _bounds.d.w,
                              _bounds.d.h + menu_height);
        }
        SDL_SetWindowMinimumSize(window, 1, menu_height + 1);

        int window_width = 0;
        int window_height = 0;
        SDL_GetWindowSize(window, &window_width, &window_height);
        self->on_native_resize(
            size(static_cast<dim>(std::max(0, window_width)),
                 static_cast<dim>(
                     std::max(1, window_height - menu_height))));

        self->on_native_create();
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
        int current_x = 0;
        int current_y = 0;
        SDL_GetWindowPosition(window, &current_x, &current_y);
        const point current(current_x, current_y);
        const point position =
            linux::sdl2::constrain_window_position(
                window, current, get_dimensions());
        if (position.x != current.x || position.y != current.y) {
            SDL_SetWindowPosition(window, position.x, position.y);
            const_cast<app_wnd *>(this)->on_native_move(position);
        }
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
        // SDL_QuitSubSystem leaves process-global platform services
        // (including SDL's Linux DBus connection) alive. The last
        // Native window owns the complete SDL runtime, so release it
        // completely and allow a later window to initialize it again.
        SDL_Quit();
    }

} // namespace native
