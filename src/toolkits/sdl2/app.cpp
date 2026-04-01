#include <native.h>
#include <bindings.h>
#include <SDL2/SDL.h>

#include "globals.h"

namespace native
{
    static void render_window_if_needed(native::wnd *wnd)
    {
        if (!wnd)
            return;

        SDL_Window *sdl_win = sdl::wnd_bindings.from_b(wnd);
        if (!sdl_win)
            return;

        auto *cache = sdl::wnd_gpx_bindings.from_a(wnd);
        if (cache && !cache->invalidated)
            return;

        int w = 0, h = 0;
        SDL_GetWindowSize(sdl_win, &w, &h);
        rect r(0, 0, static_cast<dim>(w), static_cast<dim>(h));
        auto &g = wnd->get_gpx().set_clip(r);
        cache = sdl::wnd_gpx_bindings.from_a(wnd);
        if (!cache || !cache->renderer)
            return;

        g.clear(rgba(255, 255, 255, 255));
        wnd_paint_event pe{r, g};
        wnd->on_wnd_paint.emit(pe);

        sdl::render_buttons(wnd, g);

        // Render menu bar on top if present
        if (auto *aw = dynamic_cast<native::app_wnd *>(wnd))
        {
            if (aw->menu.id())
            {
                auto *sm = sdl::menu_bindings.from_a(aw->menu.id());
                if (sm)
                    sdl::render_menu(sm, g, w, h);
            }
        }

        SDL_RenderPresent(cache->renderer);
        cache->invalidated = false;
    }

    int app::main_loop()
    {
        SDL_Event event;
        bool running = true;

        while (running)
        {
            while (SDL_PollEvent(&event))
            {
                // Handle quit before window lookup — it has no windowID.
                if (event.type == SDL_QUIT)
                {
                    running = false;
                    break;
                }

                native::wnd *wnd = sdl::wnd_bindings.from_a(
                    event.window.windowID
                        ? SDL_GetWindowFromID(event.window.windowID)
                        : sdl::main_window);

                if (!wnd)
                    continue;

                switch (event.type)
                {
                case SDL_MOUSEMOTION:
                {
                    if (auto *aw = dynamic_cast<native::app_wnd *>(wnd))
                    {
                        if (aw->menu.id())
                        {
                            auto *sm = sdl::menu_bindings.from_a(aw->menu.id());
                            int win_w = 0;
                            int win_h = 0;
                            SDL_Window *menu_sdl_win = sdl::wnd_bindings.from_b(wnd);
                            if (menu_sdl_win)
                                SDL_GetWindowSize(menu_sdl_win, &win_w, &win_h);
                            if (sm && sdl::handle_menu_motion(sm, event.motion.x, event.motion.y, win_w))
                            {
                                if (auto *cache = sdl::wnd_gpx_bindings.from_a(wnd))
                                    cache->invalidated = true;
                            }
                        }
                    }
                    sdl::handle_button_motion(wnd, event.motion.x, event.motion.y);
                    wnd->on_mouse_move.emit(point(event.motion.x, event.motion.y));
                    break;
                }

                case SDL_MOUSEBUTTONDOWN:
                case SDL_MOUSEBUTTONUP:
                {
                    auto *cache2 = sdl::wnd_gpx_bindings.from_a(wnd);

                    // Let the menu intercept down events first
                    if (event.type == SDL_MOUSEBUTTONDOWN)
                    {
                        if (auto *aw = dynamic_cast<native::app_wnd *>(wnd))
                        {
                            if (aw->menu.id())
                            {
                                auto *sm = sdl::menu_bindings.from_a(aw->menu.id());
                                int btn_win_w = 0, btn_win_h = 0;
                                SDL_Window *btn_sdl_win = sdl::wnd_bindings.from_b(wnd);
                                if (btn_sdl_win)
                                    SDL_GetWindowSize(btn_sdl_win, &btn_win_w, &btn_win_h);
                                if (sm && sdl::handle_menu_click(sm, event.button.x, event.button.y, btn_win_w))
                                {
                                    if (cache2) cache2->invalidated = true;
                                    break;
                                }
                            }
                        }
                    }

                    if (sdl::handle_button_mouse(
                            wnd,
                            event.button.x,
                            event.button.y,
                            event.type == SDL_MOUSEBUTTONDOWN,
                            event.type == SDL_MOUSEBUTTONUP))
                    {
                        if (cache2)
                            cache2->invalidated = true;
                        break;
                    }

                    mouse_button btn = mouse_button::none;
                    mouse_action act = (event.type == SDL_MOUSEBUTTONDOWN)
                                           ? mouse_action::press
                                           : mouse_action::release;
                    switch (event.button.button)
                    {
                    case SDL_BUTTON_LEFT:   btn = mouse_button::left;   break;
                    case SDL_BUTTON_RIGHT:  btn = mouse_button::right;  break;
                    case SDL_BUTTON_MIDDLE: btn = mouse_button::middle; break;
                    }

                    if (btn != mouse_button::none)
                    {
                        mouse_event me(btn, act, point(event.button.x, event.button.y));
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

                    mouse_wheel_event whe(point(), delta, dir);
                    wnd->on_mouse_wheel.emit(whe);
                    break;
                }

                case SDL_WINDOWEVENT:
                    switch (event.window.event)
                    {
                    case SDL_WINDOWEVENT_EXPOSED:
                        if (auto *cache = sdl::wnd_gpx_bindings.from_a(wnd))
                            cache->invalidated = true;
                        break;

                    case SDL_WINDOWEVENT_RESIZED:
                        if (auto *cache = sdl::wnd_gpx_bindings.from_a(wnd))
                            cache->invalidated = true;
                    {
                        size s(event.window.data1, event.window.data2);
                        wnd->on_native_resize(s);
                        wnd->on_wnd_resize.emit(s);
                    }
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

            render_window_if_needed(app::main_wnd());
            SDL_Delay(1);
        }

        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return 0;
    }

} // namespace native
