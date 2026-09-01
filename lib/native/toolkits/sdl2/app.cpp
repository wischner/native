//
// Implements the SDL2 application event-loop backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>
#include <native/app.h>
#include <bindings.h>
#include <post_backend.h>
#include <SDL2/SDL.h>

#include <algorithm>
#include <vector>

#include "globals.h"
#include "window_position.h"

namespace native
{
    using linux::sdl2::content_origin_y;

    // Recheck decorations after the compositor has presented a window.
    static void keep_window_reachable(native::wnd *owner) {
        SDL_Window *window =
            linux::sdl2::wnd_bindings.handle_from_object(owner);
        if (!window)
            return;

        int x = 0;
        int y = 0;
        SDL_GetWindowPosition(window, &x, &y);
        const point current(x, y);
        const point position =
            linux::sdl2::constrain_window_position(
                window, current, owner->get_dimensions());
        if (position.x == current.x && position.y == current.y)
            return;

        SDL_SetWindowPosition(window, position.x, position.y);
        owner->on_native_move(position);
    }

    // Recover the modal window which blocks input to this owner branch.
    static app_wnd *blocking_modal(app_wnd *window) {
        if (!window)
            return nullptr;

        app_wnd *branch = window;
        app_wnd *active = nullptr;
        for (app_wnd *ancestor = window; ancestor;
             branch = ancestor, ancestor = ancestor->get_owner()) {
            app_wnd *candidate = ancestor->get_active_modal();
            if (candidate && candidate != branch) {
                active = candidate;
                break;
            }
        }
        while (active && active->get_active_modal())
            active = active->get_active_modal();
        return active;
    }

    // Keep native modality visible even on window managers which do not
    // enforce SDL_SetWindowModalFor stacking by themselves.
    static void raise_blocking_modal(native::wnd *window) {
        auto *application_window =
            dynamic_cast<native::app_wnd *>(window);
        app_wnd *modal = blocking_modal(application_window);
        SDL_Window *modal_window = modal
            ? linux::sdl2::wnd_bindings.handle_from_object(modal)
            : nullptr;
        if (!modal_window)
            return;

        SDL_RaiseWindow(modal_window);
#if SDL_VERSION_ATLEAST(2, 0, 5)
        SDL_SetWindowInputFocus(modal_window);
#endif
    }

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

        int w = 0;
        int h = 0;
        SDL_GetWindowSize(sdl_win, &w, &h);
        const int content_y = content_origin_y(wnd);
        const int content_height = std::max(1, h - content_y);
        rect content_bounds(
            0,
            0,
            static_cast<dim>(w),
            static_cast<dim>(content_height));
        auto &g = wnd->get_gpx();
        cache = linux::sdl2::wnd_gpx_bindings.object_from_handle(wnd);
        if (!cache || !cache->renderer)
            return;

        SDL_Rect viewport = {0, content_y, w, content_height};
        SDL_RenderSetViewport(cache->renderer, &viewport);
        g.set_clip(content_bounds);
        g.clear(rgba(255, 255, 255, 255));
        wnd_paint_event pe{content_bounds, g};
        wnd->on_wnd_paint.emit(pe);

        linux::sdl2::render_buttons(wnd, g);
        linux::sdl2::render_checks(wnd, g);
        linux::sdl2::render_radios(wnd, g);
        linux::sdl2::render_lists(wnd, g);
        linux::sdl2::render_text_edits(wnd, g);
        linux::sdl2::render_collections(wnd, g);

        SDL_RenderSetViewport(cache->renderer, nullptr);

        // Render the menu in physical window coordinates above content.
        if (auto *aw = dynamic_cast<native::app_wnd *>(wnd)) {
            if (aw->menu.id()) {
                g.set_clip(rect(0,
                                0,
                                static_cast<dim>(w),
                                static_cast<dim>(h)));
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
            linux::sdl2::x11_clipboard::service();
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
                    raise_blocking_modal(wnd);
                    continue;
                }
                if (event.type == SDL_WINDOWEVENT &&
                    event.window.event ==
                        SDL_WINDOWEVENT_FOCUS_GAINED &&
                    !wnd->get_input_enabled()) {
                    raise_blocking_modal(wnd);
                    continue;
                }

                switch (event.type) {
                case SDL_KEYDOWN:
                    if (!linux::sdl2::handle_text_edit_key(
                            wnd, event.key))
                        linux::sdl2::handle_collection_key(
                            wnd, event.key);
                    break;

                case SDL_TEXTINPUT:
                    if (!linux::sdl2::handle_text_edit_input(
                            wnd, event.text.text)) {
                        linux::sdl2::handle_collection_text(
                            wnd, event.text.text);
                    }
                    break;

                case SDL_MOUSEMOTION: {
                    const int logical_y =
                        event.motion.y - content_origin_y(wnd);
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
                    if (logical_y < 0)
                        break;
                    linux::sdl2::handle_button_motion(
                        wnd, event.motion.x, logical_y);
                    linux::sdl2::handle_check_motion(
                        wnd, event.motion.x, logical_y);
                    linux::sdl2::handle_radio_motion(
                        wnd, event.motion.x, logical_y);
                    linux::sdl2::handle_text_edit_motion(
                        wnd, event.motion.x, logical_y);
                    linux::sdl2::handle_collection_motion(
                        wnd, event.motion.x, logical_y);
                    wnd->on_mouse_move.emit(
                        point(event.motion.x, logical_y));
                    break;
                }

                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP: {
                    const int logical_y =
                        event.button.y - content_origin_y(wnd);

                    // A control callback may close its window. Never
                    // retain renderer state across callback dispatch:
                    // app_wnd::destroy() releases that cache
                    // immediately.
                    const auto invalidate_live_window = [wnd]() {
                        if (!wnd || !wnd->get_created())
                            return;
                        auto *cache =
                            linux::sdl2::wnd_gpx_bindings
                                .object_from_handle(wnd);
                        if (cache)
                            cache->invalidated = true;
                    };

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
                                    invalidate_live_window();
                                    break;
                                }
                            }
                        }

                    }

                    if (logical_y < 0)
                        break;

                    if (linux::sdl2::handle_collection_mouse(
                            wnd,
                            event.button.x,
                            logical_y,
                            event.type == SDL_MOUSEBUTTONDOWN,
                            event.type == SDL_MOUSEBUTTONUP,
                            event.button.clicks)) {
                        invalidate_live_window();
                        break;
                    }

                    if (linux::sdl2::handle_text_edit_mouse(
                            wnd,
                            event.button.x,
                            logical_y,
                            event.type == SDL_MOUSEBUTTONDOWN)) {
                        invalidate_live_window();
                        break;
                    }

                    if (linux::sdl2::handle_button_mouse(
                            wnd,
                            event.button.x,
                            logical_y,
                            event.type == SDL_MOUSEBUTTONDOWN,
                            event.type == SDL_MOUSEBUTTONUP)) {
                        invalidate_live_window();
                        break;
                    }

                    if (linux::sdl2::handle_check_mouse(
                            wnd,
                            event.button.x,
                            logical_y,
                            event.type == SDL_MOUSEBUTTONDOWN,
                            event.type == SDL_MOUSEBUTTONUP) ||
                        linux::sdl2::handle_radio_mouse(
                            wnd,
                            event.button.x,
                            logical_y,
                            event.type == SDL_MOUSEBUTTONDOWN,
                            event.type == SDL_MOUSEBUTTONUP) ||
                        linux::sdl2::handle_list_mouse(
                            wnd,
                            event.button.x,
                            logical_y,
                            event.type == SDL_MOUSEBUTTONUP)) {
                        invalidate_live_window();
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
                            point(event.button.x, logical_y));
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

                    int pointer_x = 0;
                    int pointer_y = 0;
                    SDL_GetMouseState(&pointer_x, &pointer_y);
                    pointer_y -= content_origin_y(wnd);
                    if (dir == wheel_direction::vertical &&
                        linux::sdl2::handle_collection_wheel(
                            wnd,
                            pointer_x,
                            pointer_y,
                            delta * 24)) {
                        break;
                    }
                    mouse_wheel_event whe(point(pointer_x, pointer_y),
                                          delta,
                                          dir);
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
                        keep_window_reachable(wnd);
                        if (auto *cache = linux::sdl2::wnd_gpx_bindings
                                              .object_from_handle(wnd))
                            cache->invalidated = true;
                        break;

                    case SDL_WINDOWEVENT_SHOWN:
                        keep_window_reachable(wnd);
                        break;

                    case SDL_WINDOWEVENT_RESIZED:
                        if (auto *cache = linux::sdl2::wnd_gpx_bindings
                                              .object_from_handle(wnd))
                            cache->invalidated = true;
                        {
                            size s(event.window.data1,
                                   std::max(
                                       1,
                                       event.window.data2 -
                                           content_origin_y(wnd)));
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

            // Work handed over by worker threads. Drained after
            // input and before the delay, so a posted repaint is
            // presented on the next pass rather than a frame later.
            // This loop polls, so no wake routine is installed: it
            // comes back around within the delay below regardless.
            detail::drain_posted_work();

            if (app_wnd *main = app::main_wnd()) {
                if (!main->get_created())
                    running = false;
            }
            SDL_Delay(1);
        }

        linux::sdl2::x11_clipboard::shutdown();
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return 0;
    }

} // namespace native
