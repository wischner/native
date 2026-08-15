//
// Implements the X11 menu backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <string>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <native.h>
#include "globals.h"

namespace
{
    // Allocate monotonically increasing identifiers for menu models.
    uint32_t next_id() {
        static uint32_t current_id = 0;
        return ++current_id;
    }
}

namespace linux::x11
{

static int text_width_est(const std::string &s) {
    return static_cast<int>(s.size()) * 7 + 16;
}

static unsigned long alloc_rgba_or(
    Display *display,
    int screen,
    native::rgba color,
    unsigned long fallback) {
    if (!display)
        return fallback;

    XColor x_color{};
    x_color.red = static_cast<unsigned short>(color.r) * 257;
    x_color.green = static_cast<unsigned short>(color.g) * 257;
    x_color.blue = static_cast<unsigned short>(color.b) * 257;

    Colormap color_map = DefaultColormap(display, screen);
    if (XAllocColor(display, color_map, &x_color))
        return x_color.pixel;
    return fallback;
}

static const int menu_item_height   = 20;
static const int popup_width  = 180;

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

static void draw_menu_bar(x11_menu *m) {
    if (!m->bar_win || !m->gc || !linux::x11::cached_display) return;
    Display *dpy = linux::x11::cached_display;

    // Background fill
    XSetForeground(dpy, m->gc, m->bar_bg);
    XFillRectangle(dpy, m->bar_win, m->gc, 0, 0, 4096, menu_bar_height);

    // Border lines
    XSetForeground(dpy, m->gc, m->border_light);
    XDrawLine(dpy, m->bar_win, m->gc, 0, 0, 4096, 0);
    XSetForeground(dpy, m->gc, m->border_dark);
    XDrawLine(dpy, m->bar_win, m->gc, 0, menu_bar_height - 1, 4096, menu_bar_height - 1);

    // Draw menu top-level titles
    for (int i = 0; i < static_cast<int>(m->tops.size()); ++i) {
        auto &top = m->tops[i];
        bool selected = (m->open_idx == i) || (m->open_idx < 0 && m->hover_top == i);

        if (selected) {
            XSetForeground(dpy, m->gc, m->select_bg);
            XFillRectangle(dpy, m->bar_win, m->gc, top.x0, 1, top.x1 - top.x0, menu_bar_height - 2);
            XSetForeground(dpy, m->gc, m->select_fg);
        }
        else {
            XSetForeground(dpy, m->gc, m->text_fg);
        }

        XDrawString(dpy, m->bar_win, m->gc,
                    top.x0 + 8, menu_bar_height - 5,
                    top.title.c_str(), static_cast<int>(top.title.size()));
    }

    XFlush(dpy);
}

static void draw_popup(x11_menu *m) {
    if (m->open_idx < 0 || m->open_idx >= static_cast<int>(m->tops.size())) return;
    if (!m->popup_win || !m->gc || !linux::x11::cached_display) return;

    Display *dpy = linux::x11::cached_display;
    auto &top    = m->tops[m->open_idx];
    int popup_h  = static_cast<int>(top.items.size()) * menu_item_height + 2;

    // Background
    XSetForeground(dpy, m->gc, m->bar_bg);
    XFillRectangle(dpy, m->popup_win, m->gc, 0, 0, popup_width, popup_h);

    // Border
    XSetForeground(dpy, m->gc, m->border_dark);
    XDrawRectangle(dpy, m->popup_win, m->gc, 0, 0, popup_width - 1, popup_h - 1);

    // Items
    for (int i = 0; i < static_cast<int>(top.items.size()); ++i) {
        int iy = 1 + i * menu_item_height;
        if (i == m->hover_item) {
            XSetForeground(dpy, m->gc, m->select_bg);
            XFillRectangle(dpy, m->popup_win, m->gc, 1, iy, popup_width - 2, menu_item_height);
            XSetForeground(dpy, m->gc, m->select_fg);
        }
        else {
            XSetForeground(dpy, m->gc, m->text_fg);
        }
        const std::string &label = top.items[i].second;
        XDrawString(dpy, m->popup_win, m->gc,
                    8, iy + menu_item_height - 5,
                    label.c_str(), static_cast<int>(label.size()));
    }

    XFlush(dpy);
}

static void show_popup(x11_menu *m, int top_idx) {
    if (top_idx < 0 || top_idx >= static_cast<int>(m->tops.size())) return;
    Display *dpy = linux::x11::cached_display;
    if (!dpy) return;
    int screen = DefaultScreen(dpy);

    auto &top   = m->tops[top_idx];
    int popup_h = static_cast<int>(top.items.size()) * menu_item_height + 2;

    // Compute absolute screen coordinates for the popup
    Window child_ret;
    int abs_x = 0, abs_y = 0;
    XTranslateCoordinates(dpy, m->bar_win, RootWindow(dpy, screen),
                          top.x0, menu_bar_height, &abs_x, &abs_y, &child_ret);

    m->open_idx = top_idx;
    m->hover_item = -1;

    if (!m->popup_win) {
        XSetWindowAttributes attrs{};
        attrs.override_redirect = True;
        attrs.background_pixel = m->bar_bg;
        attrs.border_pixel = m->border_dark;
        attrs.event_mask = ExposureMask | ButtonPressMask | PointerMotionMask | LeaveWindowMask;
        m->popup_win = XCreateWindow(
            dpy, RootWindow(dpy, screen),
            abs_x, abs_y,
            static_cast<unsigned>(popup_width), static_cast<unsigned>(popup_h),
            1,
            CopyFromParent,
            InputOutput,
            CopyFromParent,
            CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask,
            &attrs);
        XMapRaised(dpy, m->popup_win);
    }
    else {
        XMoveResizeWindow(dpy, m->popup_win, abs_x, abs_y, popup_width, popup_h);
        XMapRaised(dpy, m->popup_win);
    }

    // Register so events on popup can find this x11_menu
    linux::x11::menu_bar_bindings.register_pair(m->popup_win, m);

    draw_menu_bar(m);
    draw_popup(m);

    // Grab on popup so clicks are still delivered to menu logic.
    XGrabPointer(dpy, m->popup_win, False,
                 ButtonPressMask, GrabModeAsync, GrabModeAsync,
                 None, None, CurrentTime);
}

static void close_popup(x11_menu *m) {
    Display *dpy = linux::x11::cached_display;
    if (!dpy) return;

    XUngrabPointer(dpy, CurrentTime);

    if (m->popup_win) {
        linux::x11::menu_bar_bindings.unregister_by_handle(m->popup_win);
        XUnmapWindow(dpy, m->popup_win);
    }

    m->open_idx = -1;
    m->hover_item = -1;
    draw_menu_bar(m);
    XFlush(dpy);
}

// ---------------------------------------------------------------------------
// Public event dispatcher (called from app.cpp event loop)
// ---------------------------------------------------------------------------

void handle_menu_bar_event(x11_menu *m, const XEvent &e) {
    Display *dpy = linux::x11::cached_display;
    if (!dpy || !m) return;

    if (e.xany.window == m->bar_win) {
        switch (e.type) {
        case Expose:
            draw_menu_bar(m);
            break;

        case MotionNotify:
        {
            int x = e.xmotion.x;
            int found = -1;
            for (int i = 0; i < static_cast<int>(m->tops.size()); ++i) {
                if (x >= m->tops[i].x0 && x < m->tops[i].x1) {
                    found = i;
                    break;
                }
            }

            if (m->hover_top != found) {
                m->hover_top = found;
                draw_menu_bar(m);
            }

            if (m->open_idx >= 0 && found >= 0 && found != m->open_idx) {
                close_popup(m);
                show_popup(m, found);
            }
            break;
        }

        case ButtonPress:
            if (e.xbutton.button == Button1) {
                int x = e.xbutton.x;
                int found = -1;
                for (int i = 0; i < static_cast<int>(m->tops.size()); ++i)
                    if (x >= m->tops[i].x0 && x < m->tops[i].x1) { found = i; break; }

                if (found >= 0) {
                    if (m->open_idx == found)
                        close_popup(m);
                    else {
                        if (m->open_idx >= 0)
                            close_popup(m);
                        show_popup(m, found);
                    }
                }
                else if (m->open_idx >= 0) {
                    close_popup(m);
                }
            }
            break;

        case LeaveNotify:
            if (m->open_idx < 0 && m->hover_top != -1) {
                m->hover_top = -1;
                draw_menu_bar(m);
            }
            break;

        default:
            break;
        }
    }
    else if (m->popup_win && e.xany.window == m->popup_win) {
        switch (e.type) {
        case Expose:
            draw_popup(m);
            break;

        case MotionNotify:
        {
            int item_idx = (e.xmotion.y - 1) / menu_item_height;
            if (m->open_idx >= 0 &&
                item_idx >= 0 &&
                item_idx < static_cast<int>(m->tops[m->open_idx].items.size())) {
                if (m->hover_item != item_idx) {
                    m->hover_item = item_idx;
                    draw_popup(m);
                }
            }
            else if (m->hover_item != -1) {
                m->hover_item = -1;
                draw_popup(m);
            }
            break;
        }

        case ButtonPress:
        {
            int y = e.xbutton.y;
            int item_idx = (y - 1) / menu_item_height;
            if (m->open_idx >= 0 &&
                item_idx >= 0 &&
                item_idx < static_cast<int>(m->tops[m->open_idx].items.size())) {
                int item_id = m->tops[m->open_idx].items[item_idx].first;
                close_popup(m);
                if (m->owner)
                    m->owner->on_menu.emit(item_id);
            }
            else {
                close_popup(m);
            }
            break;
        }

        case LeaveNotify:
            if (m->hover_item != -1) {
                m->hover_item = -1;
                draw_popup(m);
            }
            break;

        default:
            break;
        }
    }
    else {
        // Click outside via GrabPointer — close popup and replay event
        if (e.type == ButtonPress && m->open_idx >= 0) {
            close_popup(m);
            XAllowEvents(dpy, ReplayPointer, CurrentTime);
        }
    }
}

} // namespace linux::x11

// ---------------------------------------------------------------------------
// native::main_menu platform implementation
// ---------------------------------------------------------------------------

namespace native {

main_menu::~main_menu() {
    detach();
}

void main_menu::detach() {
    if (!_id) {
        _owner = nullptr;
        return;
    }

    auto *m = linux::x11::menu_bindings.object_from_handle(_id);
    if (m) {
        Display *dpy = linux::x11::cached_display;
        if (dpy) {
            if (m->popup_win) {
                linux::x11::menu_bar_bindings.unregister_by_handle(m->popup_win);
                XDestroyWindow(dpy, m->popup_win);
            }
            if (m->bar_win) {
                linux::x11::menu_bar_bindings.unregister_by_handle(m->bar_win);
                XDestroyWindow(dpy, m->bar_win);
            }
            if (m->gc)
                XFreeGC(dpy, m->gc);
        }
        delete m;
    }
    linux::x11::menu_bindings.unregister_by_handle(_id);
    _id = 0;
    _owner = nullptr;
}

void main_menu::attach(app_wnd &owner) {
    if (_id || _tops.empty()) return;
    _owner = &owner;

    Display *dpy = linux::x11::cached_display;
    if (!dpy) return;

    Window main_win = linux::x11::wnd_bindings.handle_from_object(&owner);
    if (!main_win) return;

    int screen = DefaultScreen(dpy);

    XWindowAttributes wa;
    XGetWindowAttributes(dpy, main_win, &wa);
    int win_w = wa.width;

    const auto palette = native::control_paint::native_palette();
    unsigned long bar_bg = linux::x11::alloc_rgba_or(
        dpy, screen, palette.menu_bar_bg, WhitePixel(dpy, screen));
    unsigned long border_dark = linux::x11::alloc_rgba_or(
        dpy, screen, palette.menu_popup_border, BlackPixel(dpy, screen));
    unsigned long border_light = linux::x11::alloc_rgba_or(
        dpy, screen, palette.menu_bar_line_top, WhitePixel(dpy, screen));
    unsigned long text_fg = linux::x11::alloc_rgba_or(
        dpy, screen, palette.menu_text, BlackPixel(dpy, screen));
    unsigned long select_bg = linux::x11::alloc_rgba_or(
        dpy, screen, palette.menu_hot_bg, BlackPixel(dpy, screen));
    unsigned long select_fg = linux::x11::alloc_rgba_or(
        dpy, screen, palette.menu_hot_text, WhitePixel(dpy, screen));

    // Create the menu bar as a child window at the top
    Window bar = XCreateSimpleWindow(
        dpy, main_win,
        0, 0,
        static_cast<unsigned>(win_w), linux::x11::menu_bar_height,
        0,
        border_dark,
        bar_bg);

    XSelectInput(dpy, bar, ExposureMask | ButtonPressMask | PointerMotionMask | LeaveWindowMask);
    XMapWindow(dpy, bar);

    // GC for drawing on the bar (shared for popup too)
    GC gc = XCreateGC(dpy, bar, 0, nullptr);

    // Build x11_menu structure and compute x positions
    auto *xm = new linux::x11::x11_menu();
    xm->bar_win = bar;
    xm->gc      = gc;
    xm->owner   = &owner;
    xm->bar_bg = bar_bg;
    xm->border_dark = border_dark;
    xm->border_light = border_light;
    xm->text_fg = text_fg;
    xm->select_bg = select_bg;
    xm->select_fg = select_fg;

    int x = 0;
    for (const auto &top : _tops) {
        linux::x11::x11_menu::top_entry te;
        te.title = top.title;
        te.x0    = x;
        te.x1    = x + linux::x11::text_width_est(top.title);
        x        = te.x1;
        for (const auto &item : top.items)
            te.items.push_back({item.id, item.label});
        xm->tops.push_back(std::move(te));
    }

    linux::x11::menu_bar_bindings.register_pair(bar, xm);
    _id = next_id();
    linux::x11::menu_bindings.register_pair(_id, xm);

    XFlush(dpy);
}

} // namespace native
