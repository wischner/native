#include <stdexcept>
#include <utility>

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <Xm/DrawingA.h>
#include <Xm/Protocols.h>

#include <native.h>

#include "globals.h"

namespace
{
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

    void ensure_backbuffer(native::wnd *owner, Widget canvas, int width, int height)
    {
        if (!canvas || !motif::cached_display || width <= 0 || height <= 0)
            return;

        auto *cache = motif::wnd_gpx_bindings.from_a(owner);
        if (!cache)
            return;

        if (cache->backbuffer && cache->buf_w == width && cache->buf_h == height)
            return;

        if (cache->backbuffer)
            XFreePixmap(motif::cached_display, cache->backbuffer);

        cache->backbuffer = XCreatePixmap(
            motif::cached_display,
            XtWindow(canvas),
            static_cast<unsigned int>(width),
            static_cast<unsigned int>(height),
            DefaultDepthOfScreen(XtScreen(canvas)));
        cache->buf_w = width;
        cache->buf_h = height;
    }

    void handle_canvas_event(Widget widget, XtPointer client_data, XEvent *event, Boolean *)
    {
        auto *owner = static_cast<native::app_wnd *>(client_data);
        if (!owner || !event)
            return;

        switch (event->type)
        {
        case Expose:
        {
            if (event->xexpose.count != 0)
                return;

            auto &g = owner->get_gpx();
            auto *cache = motif::wnd_gpx_bindings.from_a(owner);

            int width = 0;
            int height = 0;
            if (cache)
            {
                width = cache->buf_w;
                height = cache->buf_h;
            }

            if (width <= 0 || height <= 0)
            {
                Dimension w = 0;
                Dimension h = 0;
                XtVaGetValues(widget, XmNwidth, &w, XmNheight, &h, nullptr);
                width = static_cast<int>(w);
                height = static_cast<int>(h);
            }

            native::rect r(0, 0, static_cast<native::dim>(width), static_cast<native::dim>(height));
            g.set_clip(r);
            g.clear(g.paper());

            native::wnd_paint_event paint_event(r, g);
            owner->on_wnd_paint.emit(paint_event);

            cache = motif::wnd_gpx_bindings.from_a(owner);
            if (cache && cache->gc && cache->backbuffer)
            {
                XCopyArea(
                    motif::cached_display,
                    cache->backbuffer,
                    XtWindow(widget),
                    cache->gc,
                    0, 0,
                    static_cast<unsigned int>(cache->buf_w),
                    static_cast<unsigned int>(cache->buf_h),
                    0, 0);
                XFlush(motif::cached_display);
            }
            break;
        }

        case ConfigureNotify:
        {
            const int width = event->xconfigure.width;
            const int height = event->xconfigure.height;

            auto *cache = motif::wnd_gpx_bindings.from_a(owner);
            if (cache)
                ensure_backbuffer(owner, widget, width, height);

            owner->on_wnd_resize.emit(native::size(static_cast<native::dim>(width), static_cast<native::dim>(height)));
            break;
        }

        case MotionNotify:
            owner->on_mouse_move.emit(native::point(event->xmotion.x, event->xmotion.y));
            break;

        case ButtonPress:
        case ButtonRelease:
        {
            if (event->xbutton.button == Button4 || event->xbutton.button == Button5)
            {
                owner->on_mouse_wheel.emit(native::mouse_wheel_event(
                    native::point(event->xbutton.x, event->xbutton.y),
                    static_cast<native::coord>(event->xbutton.button == Button4 ? 1 : -1),
                    native::wheel_direction::vertical));
                return;
            }

            native::mouse_button button = decode_button(event->xbutton.button);
            if (button == native::mouse_button::none)
                return;

            owner->on_mouse_click.emit(native::mouse_event(
                button,
                event->type == ButtonPress ? native::mouse_action::press : native::mouse_action::release,
                native::point(event->xbutton.x, event->xbutton.y)));
            break;
        }

        default:
            break;
        }
    }

    void handle_shell_event(Widget, XtPointer client_data, XEvent *event, Boolean *)
    {
        auto *owner = static_cast<native::app_wnd *>(client_data);
        if (!owner || !event)
            return;

        if (event->type == ConfigureNotify)
        {
            owner->on_wnd_move.emit(native::point(event->xconfigure.x, event->xconfigure.y));
        }
    }

    void handle_wm_delete(Widget, XtPointer client_data, XtPointer)
    {
        auto *owner = static_cast<native::app_wnd *>(client_data);
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
            Widget shell = motif::shell_bindings.from_b(this);
            if (shell)
                XtVaSetValues(shell, XtNtitle, _title.c_str(), nullptr);
        }

        return *this;
    }

    const std::string &app_wnd::title() const
    {
        return _title;
    }

    void app_wnd::create() const
    {
        if (_created)
            return;

        Widget shell = nullptr;
        Display *probe_display = motif::cached_display;

        if (!motif::app_instance)
        {
            int argc = app::argc;
            char **argv = app::argv;

            shell = XtVaAppInitialize(
                &motif::app_instance,
                const_cast<char *>("Native"),
                nullptr, 0,
                &argc,
                argv,
                nullptr,
                nullptr);
            if (!shell)
                throw std::runtime_error("Motif: Failed to initialize Xt application shell.");

            if (probe_display && probe_display != XtDisplay(shell))
                XCloseDisplay(probe_display);

            motif::cached_display = XtDisplay(shell);
        }
        else
        {
            shell = XtVaAppCreateShell(
                const_cast<char *>("native"),
                const_cast<char *>("Native"),
                applicationShellWidgetClass,
                motif::cached_display,
                nullptr);
            if (!shell)
                throw std::runtime_error("Motif: Failed to create top-level shell.");
        }

        XtVaSetValues(
            shell,
            XtNx, _bounds.p.x,
            XtNy, _bounds.p.y,
            XtNwidth, _bounds.d.w,
            XtNheight, _bounds.d.h,
            XtNtitle, _title.c_str(),
            nullptr);

        Widget canvas = XmCreateDrawingArea(shell, const_cast<char *>("canvas"), nullptr, 0);
        XtVaSetValues(
            canvas,
            XmNwidth, _bounds.d.w,
            XmNheight, _bounds.d.h,
            XmNmarginWidth, 0,
            XmNmarginHeight, 0,
            nullptr);
        XtManageChild(canvas);

        XtAddEventHandler(
            canvas,
            ExposureMask | StructureNotifyMask | PointerMotionMask | ButtonPressMask | ButtonReleaseMask,
            False,
            handle_canvas_event,
            const_cast<app_wnd *>(this));
        XtAddEventHandler(
            shell,
            StructureNotifyMask,
            False,
            handle_shell_event,
            const_cast<app_wnd *>(this));

        motif::shell_bindings.register_pair(shell, const_cast<app_wnd *>(this));
        motif::wnd_bindings.register_pair(canvas, const_cast<app_wnd *>(this));

        _created = true;
        const_cast<app_wnd *>(this)->on_wnd_create.emit();
    }

    void app_wnd::show() const
    {
        if (!_created)
            throw std::runtime_error("Motif: Cannot show window before it is created.");

        app_wnd *self = const_cast<app_wnd *>(this);
        Widget shell = motif::shell_bindings.from_b(self);
        Widget canvas = motif::wnd_bindings.from_b(self);
        if (!shell)
            throw std::runtime_error("Motif: Missing shell binding for app_wnd.");
        if (!canvas)
            throw std::runtime_error("Motif: Missing drawing widget binding for app_wnd.");

        XtRealizeWidget(shell);
        motif::cached_display = XtDisplay(shell);

        // Prevent XClearArea from blanking the drawing surface before Expose.
        // The backbuffer XCopyArea should own what reaches the screen.
        XSetWindowBackgroundPixmap(motif::cached_display, XtWindow(canvas), None);

        motif::wm_delete_window_atom = XmInternAtom(motif::cached_display, const_cast<char *>("WM_DELETE_WINDOW"), False);
        XmAddWMProtocolCallback(shell, motif::wm_delete_window_atom, handle_wm_delete, self);
        XmActivateWMProtocol(shell, motif::wm_delete_window_atom);

        XMapRaised(motif::cached_display, XtWindow(shell));
        XFlush(motif::cached_display);
        invalidate();
    }

    void app_wnd::destroy() const
    {
        if (!_created)
            return;

        app_wnd *self = const_cast<app_wnd *>(this);
        Widget shell = motif::shell_bindings.from_b(self);

        if (auto *cache = motif::wnd_gpx_bindings.from_a(self))
        {
            if (cache->gc)
                XFreeGC(motif::cached_display, cache->gc);
            if (cache->backbuffer)
                XFreePixmap(motif::cached_display, cache->backbuffer);
            delete cache;
            motif::wnd_gpx_bindings.unregister_by_a(self);
        }

        motif::wnd_bindings.unregister_by_b(self);
        motif::shell_bindings.unregister_by_b(self);

        if (shell)
            XtDestroyWidget(shell);

        _created = false;

        if (self == app::main_wnd())
            motif::exit_requested = true;
    }
} // namespace native
