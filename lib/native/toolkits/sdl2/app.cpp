//
// Implements the SDL2 application event-loop backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>
#include <native/app.h>
#include <bindings.h>
#include <SDL2/SDL.h>

#include <vector>

#include "globals.h"

namespace native
{
    static void render_window_if_needed(native::wnd *wnd) {
        if (!wnd)
            return;

        SDL_Window *sdl_win =
            linux::sdl2::wnd_bindings.handle_from_object(wnd);
        if (!sdl_win)
            return;

        auto *cache =
            linux::sdl2::wnd_gpx_bindings.object_from_handle(wnd);
        if (cache && !cache->invalidated)
            return;

        int w = 0, h = 0;
        SDL_GetWindowSize(sdl_win, &w, &h);
        rect r(0, 0, static_cast<dim>(w), static_cast<dim>(h));
        auto &g = wnd->get_gpx().set_clip(r);
        cache = linux::sdl2::wnd_gpx_bindings.object_from_handle(wnd);
        if (!cache || !cache->renderer)
            return;

        g.clear(rgba(255, 255, 255, 255));
        wnd_paint_event pe{r, g};
        wnd->on_wnd_paint.emit(pe);

        linux::sdl2::render_buttons(wnd, g);
        linux::sdl2::render_checks(wnd, g);
        linux::sdl2::render_radios(wnd, g);
        linux::sdl2::render_lists(wnd, g);

        // Render menu bar on top if present
        if (auto *aw = dynamic_cast<native::app_wnd *>(wnd)) {
            if (aw->menu.id()) {
                auto *sm =
                    linux::sdl2::menu_bindings.object_from_handle(
                        aw->menu.id());
                if (sm)
                    linux::sdl2::render_menu(sm, g, w, h);
            }
        }

        SDL_RenderPresent(cache->renderer);
        cache->invalidated = false;
    }

    static bool is_input_event(Uint32 type) {
        return type == SDL_KEYDOWN || type == SDL_KEYUP ||
               type == SDL_TEXTEDITING || type == SDL_TEXTINPUT ||
               type == SDL_MOUSEMOTION ||
               type == SDL_MOUSEBUTTONDOWN ||
               type == SDL_MOUSEBUTTONUP ||
               type == SDL_MOUSEWHEEL;
    }

    int app::main_loop() {
        SDL_Event event;
        bool running = true;

        while (running) {
            while (SDL_PollEvent(&event)) {
                // Handle quit before window lookup — it has no
                // windowID.
                if (event.type == SDL_QUIT) {
                    if (app_wnd *main = app::main_wnd())
                        main->destroy();
                    running = false;
                    break;
                }

                native::wnd *wnd =
                    linux::sdl2::wnd_bindings.object_from_handle(
                        event.window.windowID
                            ? SDL_GetWindowFromID(event.window.windowID)
                            : linux::sdl2::main_window);

                if (!wnd)
                    continue;

                if (is_input_event(event.type) &&
                    !wnd->get_input_enabled()) {
                    continue;
                }

                switch (event.type) {
                case SDL_MOUSEMOTION: {
                    if (auto *aw =
                            dynamic_cast<native::app_wnd *>(wnd)) {
                        if (aw->menu.id()) {
                            auto *sm =
                                linux::sdl2::menu_bindings
                                    .object_from_handle(aw->menu.id());
                            int win_w = 0;
                            int win_h = 0;
                            SDL_Window *menu_sdl_win =
                                linux::sdl2::wnd_bindings
                                    .handle_from_object(wnd);
                            if (menu_sdl_win)
                                SDL_GetWindowSize(
                                    menu_sdl_win, &win_w, &win_h);
                            if (sm && linux::sdl2::handle_menu_motion(
                                          sm,
                                          event.motion.x,
                                          event.motion.y,
                                          win_w)) {
                                if (auto *cache =
                                        linux::sdl2::wnd_gpx_bindings
                                            .object_from_handle(wnd)) {
                                    cache->invalidated = true;
                                }
                            }
                        }
                    }
                    linux::sdl2::handle_button_motion(
                        wnd, event.motion.x, event.motion.y);
                    linux::sdl2::handle_check_motion(
                        wnd, event.motion.x, event.motion.y);
                    linux::sdl2::handle_radio_motion(
                        wnd, event.motion.x, event.motion.y);
                    wnd->on_mouse_move.emit(
                        point(event.motion.x, event.motion.y));
                    break;
                }

                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP: {
                    auto *cache2 = linux::sdl2::wnd_gpx_bindings
                                       .object_from_handle(wnd);

                    // Let the menu intercept down events first
                    if (event.type == SDL_MOUSEBUTTONDOWN) {
                        if (auto *aw =
                                dynamic_cast<native::app_wnd *>(wnd)) {
                            if (aw->menu.id()) {
                                auto *sm = linux::sdl2::menu_bindings
                                               .object_from_handle(
                                                   aw->menu.id());
                                int btn_win_w = 0, btn_win_h = 0;
                                SDL_Window *btn_sdl_win =
                                    linux::sdl2::wnd_bindings
                                        .handle_from_object(wnd);
                                if (btn_sdl_win)
                                    SDL_GetWindowSize(btn_sdl_win,
                                                      &btn_win_w,
                                                      &btn_win_h);
                                if (sm &&
                                    linux::sdl2::handle_menu_click(
                                        sm,
                                        event.button.x,
                                        event.button.y,
                                        btn_win_w)) {
                                    if (cache2)
                                        cache2->invalidated = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (linux::sdl2::handle_button_mouse(
                            wnd,
                            event.button.x,
                            event.button.y,
                            event.type == SDL_MOUSEBUTTONDOWN,
                            event.type == SDL_MOUSEBUTTONUP)) {
                        if (cache2)
                            cache2->invalidated = true;
                        break;
                    }

                    if (linux::sdl2::handle_check_mouse(
                            wnd,
                            event.button.x,
                            event.button.y,
                            event.type == SDL_MOUSEBUTTONDOWN,
                            event.type == SDL_MOUSEBUTTONUP) ||
                        linux::sdl2::handle_radio_mouse(
                            wnd,
                            event.button.x,
                            event.button.y,
                            event.type == SDL_MOUSEBUTTONDOWN,
                            event.type == SDL_MOUSEBUTTONUP) ||
                        linux::sdl2::handle_list_mouse(
                            wnd,
                            event.button.x,
                            event.button.y,
                            event.type == SDL_MOUSEBUTTONUP)) {
                        if (cache2)
                            cache2->invalidated = true;
                        break;
                    }

                    mouse_button btn = mouse_button::none;
                    mouse_action act =
                        (event.type == SDL_MOUSEBUTTONDOWN)
                            ? mouse_action::press
                            : mouse_action::release;
                    switch (event.button.button) {
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

                    if (btn != mouse_button::none) {
                        mouse_event me(
                            btn,
                            act,
                            point(event.button.x, event.button.y));
                        wnd->on_mouse_click.emit(me);
                    }
                    break;
                }

                case SDL_MOUSEWHEEL: {
                    wheel_direction dir =
                        event.wheel.x != 0 ? wheel_direction::horizontal
                                           : wheel_direction::vertical;

                    coord delta = (dir == wheel_direction::horizontal)
                                      ? event.wheel.x
                                      : event.wheel.y;

                    mouse_wheel_event whe(point(), delta, dir);
                    wnd->on_mouse_wheel.emit(whe);
                    break;
                }

                case SDL_WINDOWEVENT:
                    switch (event.window.event) {
                    case SDL_WINDOWEVENT_CLOSE:
                        wnd->destroy();
                        if (wnd == app::main_wnd())
                            running = false;
                        break;

                    case SDL_WINDOWEVENT_EXPOSED:
                        if (auto *cache = linux::sdl2::wnd_gpx_bindings
                                              .object_from_handle(wnd))
                            cache->invalidated = true;
                        break;

                    case SDL_WINDOWEVENT_RESIZED:
                        if (auto *cache = linux::sdl2::wnd_gpx_bindings
                                              .object_from_handle(wnd))
                            cache->invalidated = true;
                        {
                            size s(event.window.data1,
                                   event.window.data2);
                            wnd->on_native_resize(s);
                            wnd->on_wnd_resize.emit(s);
                        }
                        break;

                    case SDL_WINDOWEVENT_MOVED: {
                        point position(event.window.data1,
                                       event.window.data2);
                        wnd->on_native_move(position);
                        wnd->on_wnd_move.emit(position);
                        break;
                    }
                    }
                    break;

                default:
                    break;
                }
            }

            const std::vector<app_wnd *> windows =
                linux::sdl2::windows;
            for (app_wnd *window : windows) {
                if (window && window->get_created())
                    render_window_if_needed(window);
            }

            if (app_wnd *main = app::main_wnd()) {
                if (!main->get_created())
                    running = false;
            }
            SDL_Delay(1);
        }

        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return 0;
    }

} // namespace native
