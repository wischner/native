#include <string>
#include <initializer_list>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>

#include <native.h>
#include "globals.h"

namespace { uint32_t next_id() { static uint32_t c = 0; return ++c; } }

namespace x11 {

static int text_width_est(const std::string &s)
{
    return static_cast<int>(s.size()) * 7 + 16;
}

static unsigned long alloc_named_color_or(Display *dpy, int screen, const char *name, unsigned long fallback)
{
    if (!dpy || !name || !*name)
        return fallback;

    XColor exact{};
    XColor def{};
    Colormap cmap = DefaultColormap(dpy, screen);
    if (XAllocNamedColor(dpy, cmap, name, &def, &exact))
        return def.pixel;
    return fallback;
}

static std::string app_instance_name()
{
    if (!native::app::argv || !native::app::argv[0] || !*native::app::argv[0])
        return "native";

    std::string full(native::app::argv[0]);
    std::size_t slash = full.find_last_of('/');
    if (slash != std::string::npos)
        full = full.substr(slash + 1);

    if (full.empty())
        return "native";

    return full;
}

static const char *resource_for(Display *dpy, const char *instance, const char *name)
{
    if (!dpy || !instance || !*instance || !name || !*name)
        return nullptr;

    const char *v = XGetDefault(dpy, instance, name);
    return (v && *v) ? v : nullptr;
}

static const char *menu_resource(Display *dpy, std::initializer_list<const char *> names)
{
    const std::string app_name = app_instance_name();

    for (const char *n : names)
    {
        if (const char *v = resource_for(dpy, app_name.c_str(), n))
            return v;
        if (const char *v = resource_for(dpy, "native", n))
            return v;
        if (const char *v = resource_for(dpy, "Native", n))
            return v;
    }

    return nullptr;
}

static const int ITEM_H   = 20;
static const int POPUP_W  = 180;

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

static void draw_menu_bar(x11menu *m)
{
    if (!m->bar_win || !m->gc || !x11::cached_display) return;
    Display *dpy = x11::cached_display;

    // Background fill
    XSetForeground(dpy, m->gc, m->bar_bg);
    XFillRectangle(dpy, m->bar_win, m->gc, 0, 0, 4096, MENU_BAR_H);

    // Border lines
    XSetForeground(dpy, m->gc, m->border_light);
    XDrawLine(dpy, m->bar_win, m->gc, 0, 0, 4096, 0);
    XSetForeground(dpy, m->gc, m->border_dark);
    XDrawLine(dpy, m->bar_win, m->gc, 0, MENU_BAR_H - 1, 4096, MENU_BAR_H - 1);

    // Draw menu top-level titles
    for (int i = 0; i < static_cast<int>(m->tops.size()); ++i)
    {
        auto &top = m->tops[i];
        bool selected = (m->open_idx == i) || (m->open_idx < 0 && m->hover_top == i);

        if (selected)
        {
            XSetForeground(dpy, m->gc, m->select_bg);
            XFillRectangle(dpy, m->bar_win, m->gc, top.x0, 1, top.x1 - top.x0, MENU_BAR_H - 2);
            XSetForeground(dpy, m->gc, m->select_fg);
        }
        else
        {
            XSetForeground(dpy, m->gc, m->text_fg);
        }

        XDrawString(dpy, m->bar_win, m->gc,
                    top.x0 + 8, MENU_BAR_H - 5,
                    top.title.c_str(), static_cast<int>(top.title.size()));
    }

    XFlush(dpy);
}

static void draw_popup(x11menu *m)
{
    if (m->open_idx < 0 || m->open_idx >= static_cast<int>(m->tops.size())) return;
    if (!m->popup_win || !m->gc || !x11::cached_display) return;

    Display *dpy = x11::cached_display;
    auto &top    = m->tops[m->open_idx];
    int popup_h  = static_cast<int>(top.items.size()) * ITEM_H + 2;

    // Background
    XSetForeground(dpy, m->gc, m->bar_bg);
    XFillRectangle(dpy, m->popup_win, m->gc, 0, 0, POPUP_W, popup_h);

    // Border
    XSetForeground(dpy, m->gc, m->border_dark);
    XDrawRectangle(dpy, m->popup_win, m->gc, 0, 0, POPUP_W - 1, popup_h - 1);

    // Items
    for (int i = 0; i < static_cast<int>(top.items.size()); ++i)
    {
        int iy = 1 + i * ITEM_H;
        if (i == m->hover_item)
        {
            XSetForeground(dpy, m->gc, m->select_bg);
            XFillRectangle(dpy, m->popup_win, m->gc, 1, iy, POPUP_W - 2, ITEM_H);
            XSetForeground(dpy, m->gc, m->select_fg);
        }
        else
        {
            XSetForeground(dpy, m->gc, m->text_fg);
        }
        const std::string &label = top.items[i].second;
        XDrawString(dpy, m->popup_win, m->gc,
                    8, iy + ITEM_H - 5,
                    label.c_str(), static_cast<int>(label.size()));
    }

    XFlush(dpy);
}

static void show_popup(x11menu *m, int top_idx)
{
    if (top_idx < 0 || top_idx >= static_cast<int>(m->tops.size())) return;
    Display *dpy = x11::cached_display;
    if (!dpy) return;
    int screen = DefaultScreen(dpy);

    auto &top   = m->tops[top_idx];
    int popup_h = static_cast<int>(top.items.size()) * ITEM_H + 2;

    // Compute absolute screen coordinates for the popup
    Window child_ret;
    int abs_x = 0, abs_y = 0;
    XTranslateCoordinates(dpy, m->bar_win, RootWindow(dpy, screen),
                          top.x0, MENU_BAR_H, &abs_x, &abs_y, &child_ret);

    m->open_idx = top_idx;
    m->hover_item = -1;

    if (!m->popup_win)
    {
        XSetWindowAttributes attrs{};
        attrs.override_redirect = True;
        attrs.background_pixel = m->bar_bg;
        attrs.border_pixel = m->border_dark;
        attrs.event_mask = ExposureMask | ButtonPressMask | PointerMotionMask | LeaveWindowMask;
        m->popup_win = XCreateWindow(
            dpy, RootWindow(dpy, screen),
            abs_x, abs_y,
            static_cast<unsigned>(POPUP_W), static_cast<unsigned>(popup_h),
            1,
            CopyFromParent,
            InputOutput,
            CopyFromParent,
            CWOverrideRedirect | CWBackPixel | CWBorderPixel | CWEventMask,
            &attrs);
        XMapRaised(dpy, m->popup_win);
    }
    else
    {
        XMoveResizeWindow(dpy, m->popup_win, abs_x, abs_y, POPUP_W, popup_h);
        XMapRaised(dpy, m->popup_win);
    }

    // Register so events on popup can find this x11menu
    x11::menubar_bindings.register_pair(m->popup_win, m);

    draw_menu_bar(m);
    draw_popup(m);

    // Grab on popup so clicks are still delivered to menu logic.
    XGrabPointer(dpy, m->popup_win, False,
                 ButtonPressMask, GrabModeAsync, GrabModeAsync,
                 None, None, CurrentTime);
}

static void close_popup(x11menu *m)
{
    Display *dpy = x11::cached_display;
    if (!dpy) return;

    XUngrabPointer(dpy, CurrentTime);

    if (m->popup_win)
    {
        x11::menubar_bindings.unregister_by_a(m->popup_win);
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

void handle_menu_bar_event(x11menu *m, const XEvent &e)
{
    Display *dpy = x11::cached_display;
    if (!dpy || !m) return;

    if (e.xany.window == m->bar_win)
    {
        switch (e.type)
        {
        case Expose:
            draw_menu_bar(m);
            break;

        case MotionNotify:
        {
            int x = e.xmotion.x;
            int found = -1;
            for (int i = 0; i < static_cast<int>(m->tops.size()); ++i)
            {
                if (x >= m->tops[i].x0 && x < m->tops[i].x1)
                {
                    found = i;
                    break;
                }
            }

            if (m->hover_top != found)
            {
                m->hover_top = found;
                draw_menu_bar(m);
            }

            if (m->open_idx >= 0 && found >= 0 && found != m->open_idx)
            {
                close_popup(m);
                show_popup(m, found);
            }
            break;
        }

        case ButtonPress:
            if (e.xbutton.button == Button1)
            {
                int x = e.xbutton.x;
                int found = -1;
                for (int i = 0; i < static_cast<int>(m->tops.size()); ++i)
                    if (x >= m->tops[i].x0 && x < m->tops[i].x1) { found = i; break; }

                if (found >= 0)
                {
                    if (m->open_idx == found)
                        close_popup(m);
                    else
                    {
                        if (m->open_idx >= 0)
                            close_popup(m);
                        show_popup(m, found);
                    }
                }
                else if (m->open_idx >= 0)
                {
                    close_popup(m);
                }
            }
            break;

        case LeaveNotify:
            if (m->open_idx < 0 && m->hover_top != -1)
            {
                m->hover_top = -1;
                draw_menu_bar(m);
            }
            break;

        default:
            break;
        }
    }
    else if (m->popup_win && e.xany.window == m->popup_win)
    {
        switch (e.type)
        {
        case Expose:
            draw_popup(m);
            break;

        case MotionNotify:
        {
            int item_idx = (e.xmotion.y - 1) / ITEM_H;
            if (m->open_idx >= 0 &&
                item_idx >= 0 &&
                item_idx < static_cast<int>(m->tops[m->open_idx].items.size()))
            {
                if (m->hover_item != item_idx)
                {
                    m->hover_item = item_idx;
                    draw_popup(m);
                }
            }
            else if (m->hover_item != -1)
            {
                m->hover_item = -1;
                draw_popup(m);
            }
            break;
        }

        case ButtonPress:
        {
            int y = e.xbutton.y;
            int item_idx = (y - 1) / ITEM_H;
            if (m->open_idx >= 0 &&
                item_idx >= 0 &&
                item_idx < static_cast<int>(m->tops[m->open_idx].items.size()))
            {
                int item_id = m->tops[m->open_idx].items[item_idx].first;
                close_popup(m);
                if (m->owner)
                    m->owner->on_menu.emit(item_id);
            }
            else
            {
                close_popup(m);
            }
            break;
        }

        case LeaveNotify:
            if (m->hover_item != -1)
            {
                m->hover_item = -1;
                draw_popup(m);
            }
            break;

        default:
            break;
        }
    }
    else
    {
        // Click outside via GrabPointer — close popup and replay event
        if (e.type == ButtonPress && m->open_idx >= 0)
        {
            close_popup(m);
            XAllowEvents(dpy, ReplayPointer, CurrentTime);
        }
    }
}

} // namespace x11

// ---------------------------------------------------------------------------
// native::main_menu platform implementation
// ---------------------------------------------------------------------------

namespace native {

main_menu::~main_menu()
{
    if (!_id) return;
    auto *m = x11::menu_bindings.from_a(_id);
    if (m)
    {
        Display *dpy = x11::cached_display;
        if (dpy)
        {
            if (m->popup_win)
            {
                x11::menubar_bindings.unregister_by_a(m->popup_win);
                XDestroyWindow(dpy, m->popup_win);
            }
            if (m->bar_win)
            {
                x11::menubar_bindings.unregister_by_a(m->bar_win);
                XDestroyWindow(dpy, m->bar_win);
            }
            if (m->gc)
                XFreeGC(dpy, m->gc);
        }
        delete m;
    }
    x11::menu_bindings.unregister_by_a(_id);
    _id = 0;
}

void main_menu::attach(app_wnd &owner)
{
    if (_id || _tops.empty()) return;
    _owner = &owner;

    Display *dpy = x11::cached_display;
    if (!dpy) return;

    Window main_win = x11::wnd_bindings.from_b(&owner);
    if (!main_win) return;

    int screen = DefaultScreen(dpy);

    XWindowAttributes wa;
    XGetWindowAttributes(dpy, main_win, &wa);
    int win_w = wa.width;

    unsigned long fallback_bg = WhitePixel(dpy, screen);
    unsigned long bar_bg = x11::alloc_named_color_or(
        dpy, screen, x11::menu_resource(dpy, {"menuBackground", "background"}), fallback_bg);
    unsigned long border_dark = x11::alloc_named_color_or(
        dpy, screen, x11::menu_resource(dpy, {"menuBorderDark", "bottomShadowColor"}), BlackPixel(dpy, screen));
    unsigned long border_light = x11::alloc_named_color_or(
        dpy, screen, x11::menu_resource(dpy, {"menuBorderLight", "topShadowColor"}), WhitePixel(dpy, screen));
    unsigned long text_fg = x11::alloc_named_color_or(
        dpy, screen, x11::menu_resource(dpy, {"menuForeground", "foreground"}), BlackPixel(dpy, screen));
    unsigned long select_bg = x11::alloc_named_color_or(
        dpy, screen, x11::menu_resource(dpy, {"menuSelectBackground", "activeBackground"}), 0x000080UL);
    unsigned long select_fg = x11::alloc_named_color_or(
        dpy, screen, x11::menu_resource(dpy, {"menuSelectForeground", "activeForeground"}), WhitePixel(dpy, screen));

    // Create the menu bar as a child window at the top
    Window bar = XCreateSimpleWindow(
        dpy, main_win,
        0, 0,
        static_cast<unsigned>(win_w), x11::MENU_BAR_H,
        0,
        border_dark,
        bar_bg);

    XSelectInput(dpy, bar, ExposureMask | ButtonPressMask | PointerMotionMask | LeaveWindowMask);
    XMapWindow(dpy, bar);

    // GC for drawing on the bar (shared for popup too)
    GC gc = XCreateGC(dpy, bar, 0, nullptr);

    // Build x11menu structure and compute x positions
    auto *xm = new x11::x11menu();
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
    for (const auto &top : _tops)
    {
        x11::x11menu::top_entry te;
        te.title = top.title;
        te.x0    = x;
        te.x1    = x + x11::text_width_est(top.title);
        x        = te.x1;
        for (const auto &item : top.items)
            te.items.push_back({item.id, item.label});
        xm->tops.push_back(std::move(te));
    }

    x11::menubar_bindings.register_pair(bar, xm);
    _id = next_id();
    x11::menu_bindings.register_pair(_id, xm);

    XFlush(dpy);
}

} // namespace native
