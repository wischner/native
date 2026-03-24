#include <stdexcept>
#include <utility>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <X11/Xlib.h>

#include <xview/xview.h>
#include <xview/frame.h>
#include <xview/canvas.h>
#include <xview/notify.h>
#include <xview/window.h>
#include <xview/win_input.h>
#ifdef coord
#undef coord
#endif

#include <native.h>

#include "globals.h"

namespace
{
    bool trace_enabled()
    {
        static int enabled = [] {
            const char *value = std::getenv("NATIVE_OPENLOOK_TRACE");
            return (value && *value && std::strcmp(value, "0") != 0) ? 1 : 0;
        }();
        return enabled != 0;
    }

    void trace(const char *msg)
    {
        if (!trace_enabled())
            return;
        std::fprintf(stderr, "[openlook] %s\n", msg);
        std::fflush(stderr);
    }

    native::mouse_button decode_button(unsigned int button)
    {
        switch (button)
        {
        case Button1: return native::mouse_button::left;
        case Button2: return native::mouse_button::middle;
        case Button3: return native::mouse_button::right;
        default: return native::mouse_button::none;
        }
    }

    void ensure_backbuffer(native::wnd *owner, Xv_Window paint_window, int width, int height)
    {
        Display *display = reinterpret_cast<Display *>(xv_get(paint_window, XV_DISPLAY));
        if (!display)
            display = openlook::cached_display;

        if (!owner || !paint_window || !display || width <= 0 || height <= 0)
            return;
        openlook::cached_display = display;

        auto *cache = openlook::wnd_gpx_bindings.from_a(owner);
        const Window xwin = static_cast<Window>(xv_get(paint_window, XV_XID));
        if (!xwin)
            return;

        if (!cache)
        {
            cache = new openlook::openlookgpx();
            cache->gc = XCreateGC(display, xwin, 0, nullptr);
            openlook::wnd_gpx_bindings.register_pair(owner, cache);
        }

        if (cache->backbuffer && cache->buf_w == width && cache->buf_h == height)
            return;

        if (cache->backbuffer)
            XFreePixmap(display, cache->backbuffer);

        cache->backbuffer = XCreatePixmap(
            display,
            xwin,
            static_cast<unsigned int>(width),
            static_cast<unsigned int>(height),
            static_cast<unsigned int>(DefaultDepth(display, DefaultScreen(display))));
        cache->buf_w = width;
        cache->buf_h = height;

        XSetForeground(display, cache->gc, WhitePixel(display, DefaultScreen(display)));
        XFillRectangle(
            display,
            cache->backbuffer,
            cache->gc,
            0,
            0,
            static_cast<unsigned int>(width),
            static_cast<unsigned int>(height));
    }

    void repaint_owner(native::app_wnd *owner, Xv_Window paint_window)
    {
        if (!owner || !paint_window)
            return;

        Display *display = reinterpret_cast<Display *>(xv_get(paint_window, XV_DISPLAY));
        if (!display)
            display = openlook::cached_display;
        if (!display)
            return;
        openlook::cached_display = display;

        const int width = static_cast<int>(xv_get(paint_window, XV_WIDTH));
        const int height = static_cast<int>(xv_get(paint_window, XV_HEIGHT));
        if (width <= 0 || height <= 0)
            return;

        ensure_backbuffer(owner, paint_window, width, height);

        auto &g = owner->get_gpx();
        auto *cache = openlook::wnd_gpx_bindings.from_a(owner);
        if (!cache || !cache->gc || !cache->backbuffer)
            return;

        native::rect r(0, 0, static_cast<native::dim>(cache->buf_w), static_cast<native::dim>(cache->buf_h));
        g.set_clip(r);
        g.clear(g.paper());

        native::wnd_paint_event paint_event(r, g);
        owner->on_wnd_paint.emit(paint_event);

        const Window xwin = static_cast<Window>(xv_get(paint_window, XV_XID));
        if (!xwin)
            return;

        XCopyArea(
            display,
            cache->backbuffer,
            xwin,
            cache->gc,
            0,
            0,
            static_cast<unsigned int>(cache->buf_w),
            static_cast<unsigned int>(cache->buf_h),
            0,
            0);
        XFlush(display);
    }

    Notify_value handle_canvas_event(Xv_Window window, Event *event, Notify_arg arg, Notify_event_type type)
    {
        auto *owner = static_cast<native::app_wnd *>(
            openlook::canvas_bindings.from_a(reinterpret_cast<Xv_opaque>(window)));

        if (owner && event)
        {
            auto *xev = reinterpret_cast<XEvent *>(event_xevent(event));
            if (xev)
            {
                switch (xev->type)
                {
                case Expose:
                    if (xev->xexpose.count == 0)
                        repaint_owner(owner, window);
                    break;

                case ConfigureNotify:
                    ensure_backbuffer(owner, window, xev->xconfigure.width, xev->xconfigure.height);
                    owner->on_wnd_resize.emit(native::size(
                        static_cast<native::dim>(xev->xconfigure.width),
                        static_cast<native::dim>(xev->xconfigure.height)));
                    break;

                case MotionNotify:
                    owner->on_mouse_move.emit(native::point(xev->xmotion.x, xev->xmotion.y));
                    break;

                case ButtonPress:
                case ButtonRelease:
                {
                    if (xev->xbutton.button == Button4 || xev->xbutton.button == Button5)
                    {
                        if (xev->type == ButtonPress)
                        {
                            owner->on_mouse_wheel.emit(native::mouse_wheel_event(
                                native::point(xev->xbutton.x, xev->xbutton.y),
                                static_cast<native::coord>(xev->xbutton.button == Button4 ? 1 : -1),
                                native::wheel_direction::vertical));
                        }
                        break;
                    }

                    native::mouse_button button = decode_button(xev->xbutton.button);
                    if (button == native::mouse_button::none)
                        break;

                    owner->on_mouse_click.emit(native::mouse_event(
                        button,
                        xev->type == ButtonPress ? native::mouse_action::press : native::mouse_action::release,
                        native::point(xev->xbutton.x, xev->xbutton.y)));
                    break;
                }

                default:
                    break;
                }
            }
            else
            {
                switch (event_action(event))
                {
                case WIN_REPAINT:
                    repaint_owner(owner, window);
                    break;

                case WIN_RESIZE:
                {
                    const int width = static_cast<int>(xv_get(window, XV_WIDTH));
                    const int height = static_cast<int>(xv_get(window, XV_HEIGHT));
                    ensure_backbuffer(owner, window, width, height);
                    owner->on_wnd_resize.emit(native::size(
                        static_cast<native::dim>(width),
                        static_cast<native::dim>(height)));
                    break;
                }

                default:
                    break;
                }
            }
        }

        return notify_next_event_func(
            reinterpret_cast<Notify_client>(window),
            reinterpret_cast<Notify_event>(event),
            arg,
            type);
    }

    Notify_value handle_frame_event(Xv_Window window, Event *event, Notify_arg arg, Notify_event_type type)
    {
        auto *owner = static_cast<native::app_wnd *>(
            openlook::frame_bindings.from_a(reinterpret_cast<Xv_opaque>(window)));

        if (owner && event)
        {
            auto *xev = reinterpret_cast<XEvent *>(event_xevent(event));
            if (xev && xev->type == ConfigureNotify)
            {
                owner->on_wnd_move.emit(native::point(xev->xconfigure.x, xev->xconfigure.y));
            }
            else if (event_action(event) == ACTION_DISMISS && event_is_down(event))
            {
                owner->destroy();
            }
        }

        return notify_next_event_func(
            reinterpret_cast<Notify_client>(window),
            reinterpret_cast<Notify_event>(event),
            arg,
            type);
    }

    void handle_frame_done(Frame frame)
    {
        auto *owner = static_cast<native::app_wnd *>(
            openlook::frame_bindings.from_a(reinterpret_cast<Xv_opaque>(frame)));
        if (owner)
            owner->destroy();
    }
} // namespace

namespace native
{
    app_wnd::app_wnd(std::string title, coord x, coord y, dim w, dim h)
        : wnd(x, y, w, h), _title(std::move(title))
    {
    }

    app_wnd::app_wnd(const std::string &title, const point &pos, const size &dim)
        : app_wnd(title, pos.x, pos.y, dim.w, dim.h)
    {
    }

    app_wnd::app_wnd(const std::string &title, const rect &bounds)
        : app_wnd(title, bounds.p.x, bounds.p.y, bounds.d.w, bounds.d.h)
    {
    }

    app_wnd &app_wnd::set_title(const std::string &title)
    {
        _title = title;

        if (_created)
        {
            Xv_opaque frame = openlook::frame_bindings.from_b(this);
            if (frame)
                xv_set(frame, FRAME_LABEL, _title.c_str(), 0);
        }

        return *this;
    }

    const std::string &app_wnd::title() const
    {
        return _title;
    }

    void app_wnd::create() const
    {
        trace("app_wnd::create enter");

        if (_created)
        {
            trace("app_wnd::create already created");
            return;
        }

        if (!openlook::xview_initialized)
        {
            int argc = app::argc;
            char **argv = app::argv;

            trace("before xv_init");
            xv_init(XV_INIT_ARGC_PTR_ARGV, &argc, argv, 0);
            trace("after xv_init");
            openlook::xview_initialized = true;
        }

        trace("before xv_create frame");
        Frame frame = static_cast<Frame>(xv_create(
            XV_NULL,
            FRAME,
            XV_X, _bounds.p.x,
            XV_Y, _bounds.p.y,
            XV_WIDTH, _bounds.d.w,
            XV_HEIGHT, _bounds.d.h,
            FRAME_LABEL, _title.c_str(),
            FRAME_NO_CONFIRM, TRUE,
            FRAME_DONE_PROC, handle_frame_done,
            0));
        trace("after xv_create frame");

        if (!frame)
            throw std::runtime_error("OpenLook: Failed to create frame.");

        openlook::cached_display = reinterpret_cast<Display *>(xv_get(frame, XV_DISPLAY));
        if (!openlook::cached_display)
        {
            xv_destroy_safe(frame);
            throw std::runtime_error("OpenLook: Failed to obtain display from frame.");
        }

        trace("before xv_create canvas");
        Canvas canvas = static_cast<Canvas>(xv_create(
            frame,
            CANVAS,
            XV_WIDTH, _bounds.d.w,
            XV_HEIGHT, _bounds.d.h,
            CANVAS_X_PAINT_WINDOW, TRUE,
            CANVAS_AUTO_CLEAR, FALSE,
            0));
        trace("after xv_create canvas");

        if (!canvas)
        {
            xv_destroy_safe(frame);
            throw std::runtime_error("OpenLook: Failed to create canvas.");
        }

        Xv_Window paint_window = canvas_paint_window(canvas);
        if (!paint_window)
        {
            xv_destroy_safe(frame);
            throw std::runtime_error("OpenLook: Failed to get canvas paint window.");
        }

        xv_set(
            paint_window,
            WIN_CONSUME_EVENTS,
            WIN_REPAINT,
            WIN_RESIZE,
            LOC_MOVE,
            LOC_DRAG,
            ACTION_SELECT,
            ACTION_ADJUST,
            ACTION_MENU,
            BUT(4),
            BUT(5),
            0,
            0);

        openlook::frame_bindings.register_pair(reinterpret_cast<Xv_opaque>(frame), const_cast<app_wnd *>(this));
        openlook::canvas_bindings.register_pair(reinterpret_cast<Xv_opaque>(paint_window), const_cast<app_wnd *>(this));

        if (notify_interpose_event_func(
                reinterpret_cast<Notify_client>(paint_window),
                reinterpret_cast<Notify_func>(handle_canvas_event),
                NOTIFY_SAFE) != NOTIFY_OK)
        {
            openlook::canvas_bindings.unregister_by_b(const_cast<app_wnd *>(this));
            openlook::frame_bindings.unregister_by_b(const_cast<app_wnd *>(this));
            xv_destroy_safe(frame);
            throw std::runtime_error("OpenLook: Failed to register canvas event handler.");
        }

        if (notify_interpose_event_func(
                reinterpret_cast<Notify_client>(frame),
                reinterpret_cast<Notify_func>(handle_frame_event),
                NOTIFY_SAFE) != NOTIFY_OK)
        {
            openlook::canvas_bindings.unregister_by_b(const_cast<app_wnd *>(this));
            openlook::frame_bindings.unregister_by_b(const_cast<app_wnd *>(this));
            xv_destroy_safe(frame);
            throw std::runtime_error("OpenLook: Failed to register frame event handler.");
        }

        _created = true;
        trace("app_wnd::create done");
        const_cast<app_wnd *>(this)->on_wnd_create.emit();
    }

    void app_wnd::show() const
    {
        trace("app_wnd::show enter");
        if (!_created)
            throw std::runtime_error("OpenLook: Cannot show window before it is created.");

        Xv_opaque frame = openlook::frame_bindings.from_b(const_cast<app_wnd *>(this));
        Xv_opaque paint_window = openlook::canvas_bindings.from_b(const_cast<app_wnd *>(this));
        if (!frame || !paint_window)
            throw std::runtime_error("OpenLook: Missing frame/canvas bindings for app_wnd.");

        xv_set(frame, XV_SHOW, TRUE, 0);
        trace("app_wnd::show xv_set XV_SHOW done");

        openlook::cached_display = reinterpret_cast<Display *>(xv_get(frame, XV_DISPLAY));
        if (!openlook::cached_display)
            throw std::runtime_error("OpenLook: Missing display handle after showing frame.");

        const Window xwin = static_cast<Window>(xv_get(paint_window, XV_XID));
        if (xwin)
            XSetWindowBackgroundPixmap(openlook::cached_display, xwin, None);

        XFlush(openlook::cached_display);

        invalidate();
        trace("app_wnd::show done");
    }

    void app_wnd::destroy() const
    {
        if (!_created)
            return;

        auto *self = const_cast<app_wnd *>(this);
        Xv_opaque frame = openlook::frame_bindings.from_b(self);
        Display *display = openlook::cached_display;
        if (!display && frame)
            display = reinterpret_cast<Display *>(xv_get(frame, XV_DISPLAY));

        if (auto *cache = openlook::wnd_gpx_bindings.from_a(self))
        {
            if (cache->gc && display)
                XFreeGC(display, cache->gc);
            if (cache->backbuffer && display)
                XFreePixmap(display, cache->backbuffer);
            delete cache;
            openlook::wnd_gpx_bindings.unregister_by_a(self);
        }

        openlook::canvas_bindings.unregister_by_b(self);
        openlook::frame_bindings.unregister_by_b(self);

        _created = false;

        if (frame)
            xv_destroy_safe(reinterpret_cast<Xv_object>(frame));

        if (self == app::main_wnd())
        {
            openlook::exit_requested = true;
            notify_stop();
        }
    }
} // namespace native
