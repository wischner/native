//
// Implements the X11 button-control backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>
#include <utility>

#include <X11/Xlib.h>

#include <native.h>

#include "globals.h"

namespace linux::x11
{
    static void ensure_button_backbuffer(native::button *owner, int w, int h) {
        if (!owner || !cached_display)
            return;

        auto *cache = wnd_gpx_bindings.object_from_handle(owner);
        Window win = wnd_bindings.handle_from_object(owner);
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

    static void draw_button(x11_button *b) {
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
                            b->owner->get_text(),
                            st);

        auto *cache = wnd_gpx_bindings.object_from_handle(b->owner);
        if (cache && cache->backbuffer && cache->gc) {
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

    void handle_button_event(native::button *b, const XEvent &e) {
        auto *h = button_bindings.object_from_handle(b);
        if (!h)
            return;

        switch (e.type) {
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
            if (e.xbutton.button == Button1) {
                h->pressed = true;
                draw_button(h);
            }
            break;

        case ButtonRelease:
            if (e.xbutton.button == Button1) {
                const bool was_pressed = h->pressed;
                h->pressed = false;
                draw_button(h);

                if (was_pressed) {
                    XWindowAttributes attrs;
                    XGetWindowAttributes(cached_display, h->win, &attrs);
                    if (e.xbutton.x >= 0 && e.xbutton.y >= 0 &&
                        e.xbutton.x < attrs.width && e.xbutton.y < attrs.height) {
                        b->on_click.emit();
                    }
                }
            }
            break;

        default:
            break;
        }
    }
} // namespace linux::x11

namespace native
{
    void button::apply_text() {
        auto *binding =
            linux::x11::button_bindings.object_from_handle(this);
        if (!binding || !binding->win)
            throw std::runtime_error(
                "X11: Missing button window binding.");

        invalidate();
    }

    void button::create() const {
        if (_created)
            return;

        if (!linux::x11::cached_display)
            throw std::runtime_error("X11: button requires an active display.");

        wnd *p = get_parent();
        if (!p)
            throw std::runtime_error("X11: button requires a parent window.");
        if (!p->get_created())
            throw std::runtime_error(
                "X11: button parent is not created.");

        Window parent_win = linux::x11::wnd_bindings.handle_from_object(p);
        if (!parent_win)
            throw std::runtime_error("X11: button parent is not created.");

        int screen = DefaultScreen(linux::x11::cached_display);
        Window btn = XCreateSimpleWindow(
            linux::x11::cached_display,
            parent_win,
            _bounds.p.x,
            _bounds.p.y,
            _bounds.d.w,
            _bounds.d.h,
            1,
            BlackPixel(linux::x11::cached_display, screen),
            WhitePixel(linux::x11::cached_display, screen));

        if (!btn)
            throw std::runtime_error("X11: Failed to create button window.");

        XSelectInput(linux::x11::cached_display, btn,
                     ExposureMask | StructureNotifyMask |
                     EnterWindowMask | LeaveWindowMask |
                     ButtonPressMask | ButtonReleaseMask);

        auto *self = const_cast<button *>(this);
        linux::x11::wnd_bindings.register_pair(btn, self);

        auto *h = new linux::x11::x11_button();
        h->win = btn;
        h->gc = XCreateGC(linux::x11::cached_display, btn, 0, nullptr);
        h->owner = self;

        XSetWindowBackground(
            linux::x11::cached_display,
            btn,
            WhitePixel(linux::x11::cached_display, screen));
        XSetWindowBorder(
            linux::x11::cached_display,
            btn,
            BlackPixel(linux::x11::cached_display, screen));

        linux::x11::button_bindings.register_pair(self, h);

        _created = true;
        self->on_wnd_create.emit();
    }

    void button::show() const {
        if (!_created)
            throw std::runtime_error("X11: Cannot show button before it is created.");

        auto *h = linux::x11::button_bindings.object_from_handle(const_cast<button *>(this));
        if (!linux::x11::cached_display || !h || !h->win)
            throw std::runtime_error("X11: Missing button window binding.");

        XMapWindow(linux::x11::cached_display, h->win);
        XFlush(linux::x11::cached_display);
    }

    void button::destroy() const {
        if (!_created)
            return;

        auto *self = const_cast<button *>(this);
        auto *h = linux::x11::button_bindings.object_from_handle(self);
        self->on_native_destroy();

        if (h) {
            if (h->gc && linux::x11::cached_display)
                XFreeGC(linux::x11::cached_display, h->gc);
            if (h->win) {
                if (linux::x11::cached_display)
                    XDestroyWindow(linux::x11::cached_display, h->win);
                linux::x11::wnd_bindings.unregister_by_handle(h->win);
            }
            linux::x11::button_bindings.unregister_by_handle(self);
            delete h;
        }
    }
} // namespace native
