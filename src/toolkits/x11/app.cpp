#include <stdexcept>

#include <X11/Xlib.h>

#include <native.h>
#include <bindings.h>

#include "globals.h"

namespace native
{

    int app::main_loop()
    {
        if (!x11::cached_display)
            throw std::runtime_error("X11: No display available for main loop.");

        XEvent event;
        bool running = true;
        native::wnd *wnd;

        while (running)
        {
            XNextEvent(x11::cached_display, &event);

            wnd = x11::wnd_bindings.from_a(event.xany.window);
            if (!wnd)
                continue;

            switch (event.type)
            {
            case Expose:
                {
                    // Just emit paint event, cache already initialized in create()
                    rect r(event.xexpose.x, event.xexpose.y, event.xexpose.width, event.xexpose.height);
                    auto &g = wnd->get_gpx().set_clip(r); 
                    wnd_paint_event e{r, g};
                    wnd->on_wnd_paint.emit(e);
                }
                break;

            case ConfigureNotify:
                wnd->on_wnd_resize.emit(size(event.xconfigure.width, event.xconfigure.height));
                wnd->on_wnd_move.emit(point(event.xconfigure.x, event.xconfigure.y));
                break;

            case MotionNotify:
                wnd->on_mouse_move.emit(point(event.xmotion.x, event.xmotion.y));
                break;

            case ButtonPress:
            case ButtonRelease:
                {
                    mouse_button btn = mouse_button::none;

                    switch (event.xbutton.button)
                    {
                    case Button1: btn = mouse_button::left; break;
                    case Button2: btn = mouse_button::middle; break;
                    case Button3: btn = mouse_button::right; break;

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

                    if (btn != mouse_button::none)
                    {
                        mouse_event e(btn, point(event.xbutton.x, event.xbutton.y));
                        wnd->on_mouse_click.emit(e);
                    }
                }
                break;

            case DestroyNotify:
                if (wnd)
                {
                    wnd->destroy();
                    if (wnd == app::main_wnd())
                        running = false;
                }
                break;

            case ClientMessage:
                if (event.xclient.data.l[0] == static_cast<long>(x11::wm_delete_window_atom))
                {
                    // Let the window handle destruction
                    wnd->destroy();
                    running = false;
                }
                break;

            default:
                break;
            }
        }

        if (x11::cached_display)
        {
            XCloseDisplay(x11::cached_display);
            x11::cached_display = nullptr;
        }

        return 0;
    }


} // namespace native