//
// Implements the X11 application event-loop backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <X11/Xlib.h>

#include <native.h>
#include <bindings.h>

#include "globals.h"

namespace native
{

    int app::main_loop() {
        if (!linux::x11::cached_display)
            throw std::runtime_error("X11: No display available for main loop.");

        XEvent event;
        bool running = true;
        native::wnd *wnd;

        while (running) {
            XNextEvent(linux::x11::cached_display, &event);

            // Check if this event belongs to a menu bar or popup window.
            {
                auto *xmenu = linux::x11::menu_bar_bindings.object_from_handle(event.xany.window);
                if (xmenu) {
                    linux::x11::handle_menu_bar_event(xmenu, event);
                    continue;
                }
            }

            wnd = linux::x11::wnd_bindings.object_from_handle(event.xany.window);
            if (!wnd)
                continue;

            if (auto *btn = dynamic_cast<native::button *>(wnd)) {
                if (event.type == DestroyNotify || event.type == ClientMessage) {
                    // Let generic destruction flow handle lifecycle teardown.
                }
                else {
                    linux::x11::handle_button_event(btn, event);
                    continue;
                }
            }

            switch (event.type) {
            case Expose:
            {
                // Drain any additional queued Expose events for this window
                // so we only repaint once for the latest state.
                {
                    XEvent discard;
                    while (XCheckTypedWindowEvent(linux::x11::cached_display,
                                                  event.xany.window, Expose, &discard)) {}
                }

                // Always repaint the full backbuffer regardless of the Expose
                // rectangle — partial repaints cause artifacts when the clip
                // region doesn't cover the whole window.
                auto &g = wnd->get_gpx();
                auto *cache = linux::x11::wnd_gpx_bindings.object_from_handle(wnd);
                rect r(0, 0,
                       cache ? cache->buf_w : 0,
                       cache ? cache->buf_h : 0);
                g.set_clip(r);

                // Clear the full backbuffer to white, then let the user paint.
                g.clear(rgba(255, 255, 255, 255));
                wnd_paint_event e{r, g};
                wnd->on_wnd_paint.emit(e);

                // Present full backbuffer to window in one fast blit.
                if (cache && cache->backbuffer) {
                    XCopyArea(linux::x11::cached_display,
                              cache->backbuffer, event.xany.window, cache->gc,
                              0, 0, cache->buf_w, cache->buf_h,
                              0, 0);
                    XFlush(linux::x11::cached_display);
                }
            }
            break;

            case ConfigureNotify:
            {
                size s(event.xconfigure.width, event.xconfigure.height);
                point position(event.xconfigure.x, event.xconfigure.y);
                wnd->on_native_resize(s);
                wnd->on_native_move(position);
                wnd->on_wnd_resize.emit(s);
                wnd->on_wnd_move.emit(position);
                // Resize menu bar if present
                if (auto *aw = dynamic_cast<native::app_wnd *>(wnd)) {
                    if (aw->menu.id()) {
                        auto *xm = linux::x11::menu_bindings.object_from_handle(aw->menu.id());
                        if (xm && xm->bar_win && linux::x11::cached_display)
                            XResizeWindow(linux::x11::cached_display, xm->bar_win,
                                          static_cast<unsigned>(event.xconfigure.width),
                                          linux::x11::menu_bar_height);
                    }
                }
                {
                    // Recreate the backbuffer whenever the window is resized.
                    auto *cache = linux::x11::wnd_gpx_bindings.object_from_handle(wnd);
                    if (cache && cache->backbuffer &&
                        (event.xconfigure.width  != cache->buf_w ||
                         event.xconfigure.height != cache->buf_h)) {
                        Display *display = linux::x11::cached_display;
                        int screen = DefaultScreen(display);
                        int nw = event.xconfigure.width;
                        int nh = event.xconfigure.height;

                        XFreePixmap(display, cache->backbuffer);
                        cache->backbuffer = XCreatePixmap(display, event.xany.window,
                                                          nw, nh,
                                                          DefaultDepth(display, screen));
                        cache->buf_w = nw;
                        cache->buf_h = nh;

                        XSetForeground(display, cache->gc, WhitePixel(display, screen));
                        XFillRectangle(display, cache->backbuffer, cache->gc,
                                       0, 0, nw, nh);
                    }
                }
                break;
            }

            case MotionNotify:
                wnd->on_mouse_move.emit(point(event.xmotion.x, event.xmotion.y));
                break;

            case ButtonPress:
            case ButtonRelease:
            {
                mouse_button btn = mouse_button::none;
                mouse_action act = (event.type == ButtonPress)
                                       ? mouse_action::press
                                       : mouse_action::release;

                switch (event.xbutton.button) {
                case Button1: btn = mouse_button::left;   break;
                case Button2: btn = mouse_button::middle; break;
                case Button3: btn = mouse_button::right;  break;

                case Button4:
                case Button5:
                {
                    mouse_wheel_event wheel(
                        point(event.xbutton.x, event.xbutton.y),
                        (event.xbutton.button == Button4 ? 1 : -1),
                        wheel_direction::vertical);
                    wnd->on_mouse_wheel.emit(wheel);
                    break;
                }

                default: break;
                }

                if (btn != mouse_button::none) {
                    mouse_event e(btn, act, point(event.xbutton.x, event.xbutton.y));
                    wnd->on_mouse_click.emit(e);
                }
            }
            break;

            case DestroyNotify:
                if (wnd) {
                    wnd->destroy();
                    if (wnd == app::main_wnd())
                        running = false;
                }
                break;

            case ClientMessage:
                if (event.xclient.data.l[0] ==
                    static_cast<long>(
                        linux::x11::wm_delete_window_atom)) {
                    wnd->destroy();
                    running = false;
                }
                break;

            default:
                break;
            }
        }

        if (linux::x11::cached_display) {
            XCloseDisplay(linux::x11::cached_display);
            linux::x11::cached_display = nullptr;
        }

        return 0;
    }


} // namespace native
