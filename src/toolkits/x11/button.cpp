#include <stdexcept>
#include <utility>

#include <X11/Xlib.h>

#include <native.h>

#include "globals.h"

namespace x11
{
    static void ensure_button_backbuffer(native::button *owner, int w, int h)
    {
        if (!owner || !cached_display)
            return;

        auto *cache = wnd_gpx_bindings.from_a(owner);
        Window win = wnd_bindings.from_b(owner);
        if (!cache || !win || !cache->backbuffer)
            return;

        if (cache->buf_w == w && cache->buf_h == h)
            return;

        int screen = DefaultScreen(cached_display);
        XFreePixmap(cached_display, cache->backbuffer);
        cache->backbuffer = XCreatePixmap(
            cached_display, win,
            static_cast<unsigned int>(w),
            static_cast<unsigned int>(h),
            DefaultDepth(cached_display, screen));
        cache->buf_w = w;
        cache->buf_h = h;
    }

    static void draw_button(x11button *b)
    {
        if (!b || !cached_display || !b->win || !b->gc || !b->owner)
            return;

        XWindowAttributes attrs;
        XGetWindowAttributes(cached_display, b->win, &attrs);

        const int w = attrs.width;
        const int h = attrs.height;
        ensure_button_backbuffer(b->owner, w, h);

        auto &g = b->owner->get_gpx();
        g.set_clip(native::rect(0, 0,
                                static_cast<native::dim>(w),
                                static_cast<native::dim>(h)));

        native::control_paint painter(g);
        native::control_paint::state st;
        st.hot = b->hover;
        st.pressed = b->pressed;
        painter.draw_button(native::rect(0, 0,
                                         static_cast<native::dim>(w),
                                         static_cast<native::dim>(h)),
                            b->owner->text(),
                            st);

        auto *cache = wnd_gpx_bindings.from_a(b->owner);
        if (cache && cache->backbuffer && cache->gc)
        {
            XCopyArea(cached_display,
                      cache->backbuffer,
                      b->win,
                      cache->gc,
                      0, 0,
                      static_cast<unsigned int>(w),
                      static_cast<unsigned int>(h),
                      0, 0);
        }

        XFlush(cached_display);
    }

    void handle_button_event(native::button *b, const XEvent &e)
    {
        auto *h = button_bindings.from_a(b);
        if (!h)
            return;

        switch (e.type)
        {
        case Expose:
            if (e.xexpose.count == 0)
                draw_button(h);
            break;

        case ConfigureNotify:
            draw_button(h);
            break;

        case EnterNotify:
            h->hover = true;
            draw_button(h);
            break;

        case LeaveNotify:
            h->hover = false;
            draw_button(h);
            break;

        case ButtonPress:
            if (e.xbutton.button == Button1)
            {
                h->pressed = true;
                draw_button(h);
            }
            break;

        case ButtonRelease:
            if (e.xbutton.button == Button1)
            {
                const bool was_pressed = h->pressed;
                h->pressed = false;
                draw_button(h);

                if (was_pressed)
                {
                    XWindowAttributes attrs;
                    XGetWindowAttributes(cached_display, h->win, &attrs);
                    if (e.xbutton.x >= 0 && e.xbutton.y >= 0 &&
                        e.xbutton.x < attrs.width && e.xbutton.y < attrs.height)
                    {
                        b->on_click.emit();
                    }
                }
            }
            break;

        default:
            break;
        }
    }
} // namespace x11

namespace native
{
    button::button(std::string text, coord x, coord y, dim w, dim h)
        : wnd(x, y, w, h), _text(std::move(text))
    {
    }

    button::button(const std::string &text, const point &pos, const size &dim)
        : button(text, pos.x, pos.y, dim.w, dim.h)
    {
    }

    button::button(const std::string &text, const rect &bounds)
        : button(text, bounds.p.x, bounds.p.y, bounds.d.w, bounds.d.h)
    {
    }

    const std::string &button::text() const
    {
        return _text;
    }

    button &button::set_text(const std::string &text)
    {
        _text = text;

        if (_created)
            invalidate();

        return *this;
    }

    void button::create() const
    {
        if (_created)
            return;

        if (!x11::cached_display)
            throw std::runtime_error("X11: button requires an active display.");

        wnd *p = parent();
        if (!p)
            throw std::runtime_error("X11: button requires a parent window.");

        Window parent_win = x11::wnd_bindings.from_b(p);
        if (!parent_win)
            throw std::runtime_error("X11: button parent is not created.");

        int screen = DefaultScreen(x11::cached_display);
        Window btn = XCreateSimpleWindow(
            x11::cached_display,
            parent_win,
            _bounds.p.x,
            _bounds.p.y,
            _bounds.d.w,
            _bounds.d.h,
            1,
            BlackPixel(x11::cached_display, screen),
            WhitePixel(x11::cached_display, screen));

        if (!btn)
            throw std::runtime_error("X11: Failed to create button window.");

        XSelectInput(x11::cached_display, btn,
                     ExposureMask | StructureNotifyMask |
                     EnterWindowMask | LeaveWindowMask |
                     ButtonPressMask | ButtonReleaseMask);

        auto *self = const_cast<button *>(this);
        x11::wnd_bindings.register_pair(btn, self);

        auto *h = new x11::x11button();
        h->win = btn;
        h->gc = XCreateGC(x11::cached_display, btn, 0, nullptr);
        h->owner = self;

        XSetWindowBackground(x11::cached_display, btn, WhitePixel(x11::cached_display, screen));
        XSetWindowBorder(x11::cached_display, btn, BlackPixel(x11::cached_display, screen));

        x11::button_bindings.register_pair(self, h);

        _created = true;
        self->on_wnd_create.emit();
    }

    void button::show() const
    {
        if (!_created)
            throw std::runtime_error("X11: Cannot show button before it is created.");

        auto *h = x11::button_bindings.from_a(const_cast<button *>(this));
        if (!h || !h->win)
            throw std::runtime_error("X11: Missing button window binding.");

        XMapWindow(x11::cached_display, h->win);
        XFlush(x11::cached_display);
    }

    void button::destroy() const
    {
        if (!_created)
            return;

        auto *self = const_cast<button *>(this);
        auto *h = x11::button_bindings.from_a(self);

        if (h)
        {
            if (h->gc && x11::cached_display)
                XFreeGC(x11::cached_display, h->gc);
            if (h->win)
            {
                if (x11::cached_display)
                    XDestroyWindow(x11::cached_display, h->win);
                x11::wnd_bindings.unregister_by_a(h->win);
            }
            x11::button_bindings.unregister_by_a(self);
            delete h;
        }

        _created = false;
    }
} // namespace native
