#include <iostream>
#include <X11/Xlib.h>

#include <native.h>
#include <bindings.h>
#include "globals.h"

namespace native
{
    int app::main_loop()
    {
        if (!x11::cached_display)
        {
            std::cerr << "X11: No display available for main loop.\n";
            return 1;
        }

        XEvent event;
        bool running = true;

        while (running)
        {
            XNextEvent(x11::cached_display, &event);

            native::wnd *wnd = x11::wnd_bindings.from_a(event.xany.window);
            if (!wnd)
                continue;

            switch (event.type)
            {
            case Expose:
                // Trigger repaint
                // (You'll later emit on_paint(graphics&) here)
                break;

            case ConfigureNotify:
                // Window resized or moved
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
                case Button1:
                    btn = mouse_button::left;
                    break;
                case Button2:
                    btn = mouse_button::middle;
                    break;
                case Button3:
                    btn = mouse_button::right;
                    break;
                case Button4: /* wheel up */
                case Button5: /* wheel down */
                {
                    mouse_wheel_event wheel(
                        point(event.xbutton.x, event.xbutton.y),
                        (event.xbutton.button == Button4 ? 1 : -1),
                        wheel_direction::vertical);
                    wnd->on_mouse_wheel.emit(wheel);
                    break;
                }
                default:
                    break;
                }

                if (btn != mouse_button::none)
                {
                    mouse_event e(btn, point(event.xbutton.x, event.xbutton.y));
                    wnd->on_mouse_click.emit(e);
                }

                // Optional: Handle wheel (Button4/5) separately below
                break;
            }

            case ClientMessage:
                // Handle WM_DELETE_WINDOW (later)
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
}
