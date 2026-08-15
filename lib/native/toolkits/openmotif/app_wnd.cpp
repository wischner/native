//
// Implements the OpenMotif application-window backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>
#include <utility>

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <Xm/DrawingA.h>
#include <Xm/MainW.h>
#include <Xm/Protocols.h>

#include <native.h>

#include "globals.h"

namespace
{
    native::mouse_button decode_button(unsigned int button) {
        switch (button) {
        case Button1: return native::mouse_button::left;
        case Button2: return native::mouse_button::middle;
        case Button3: return native::mouse_button::right;
        default: return native::mouse_button::none;
        }
    }

    void ensure_backbuffer(native::wnd *owner, Widget canvas, int width, int height) {
        if (!canvas || !linux::openmotif::cached_display || width <= 0 || height <= 0)
            return;

        auto *cache = linux::openmotif::wnd_gpx_bindings.object_from_handle(owner);
        if (!cache)
            return;

        if (cache->backbuffer && cache->buf_w == width && cache->buf_h == height)
            return;

        if (cache->backbuffer)
            XFreePixmap(linux::openmotif::cached_display, cache->backbuffer);

        cache->backbuffer = XCreatePixmap(
            linux::openmotif::cached_display,
            XtWindow(canvas),
            static_cast<unsigned int>(width),
            static_cast<unsigned int>(height),
            DefaultDepthOfScreen(XtScreen(canvas)));
        cache->buf_w = width;
        cache->buf_h = height;
    }

    void handle_canvas_event(Widget widget, XtPointer client_data, XEvent *event, Boolean *) {
        auto *owner = static_cast<native::app_wnd *>(client_data);
        if (!owner || !event)
            return;

        switch (event->type) {
        case Expose:
        {
            if (event->xexpose.count != 0)
                return;

            auto &g = owner->get_gpx();
            auto *cache = linux::openmotif::wnd_gpx_bindings.object_from_handle(owner);

            int width = 0;
            int height = 0;
            if (cache) {
                width = cache->buf_w;
                height = cache->buf_h;
            }

            if (width <= 0 || height <= 0) {
                Dimension w = 0;
                Dimension h = 0;
                XtVaGetValues(widget, XmNwidth, &w, XmNheight, &h, nullptr);
                width = static_cast<int>(w);
                height = static_cast<int>(h);
            }

            native::rect r(0, 0, static_cast<native::dim>(width), static_cast<native::dim>(height));
            g.set_clip(r);
            g.clear(g.get_paper());

            native::wnd_paint_event paint_event(r, g);
            owner->on_wnd_paint.emit(paint_event);

            cache = linux::openmotif::wnd_gpx_bindings.object_from_handle(owner);
            if (cache && cache->gc && cache->backbuffer) {
                XCopyArea(
                    linux::openmotif::cached_display,
                    cache->backbuffer,
                    XtWindow(widget),
                    cache->gc,
                    0, 0,
                    static_cast<unsigned int>(cache->buf_w),
                    static_cast<unsigned int>(cache->buf_h),
                    0, 0);
                XFlush(linux::openmotif::cached_display);
            }
            break;
        }

        case ConfigureNotify:
        {
            const int width = event->xconfigure.width;
            const int height = event->xconfigure.height;

            auto *cache = linux::openmotif::wnd_gpx_bindings.object_from_handle(owner);
            if (cache)
                ensure_backbuffer(owner, widget, width, height);

            native::size s(static_cast<native::dim>(width), static_cast<native::dim>(height));
            owner->on_native_resize(s);
            owner->on_wnd_resize.emit(s);
            break;
        }

        case MotionNotify:
            owner->on_mouse_move.emit(native::point(event->xmotion.x, event->xmotion.y));
            break;

        case ButtonPress:
        case ButtonRelease:
        {
            if (event->xbutton.button == Button4 || event->xbutton.button == Button5) {
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
                event->type == ButtonPress
                    ? native::mouse_action::press
                    : native::mouse_action::release,
                native::point(event->xbutton.x, event->xbutton.y)));
            break;
        }

        default:
            break;
        }
    }

    void handle_shell_event(Widget, XtPointer client_data, XEvent *event, Boolean *) {
        auto *owner = static_cast<native::app_wnd *>(client_data);
        if (!owner || !event)
            return;

        if (event->type == ConfigureNotify) {
            native::point position(
                event->xconfigure.x,
                event->xconfigure.y);
            owner->on_native_move(position);
            owner->on_wnd_move.emit(position);
        }
    }

    void handle_wm_delete(Widget, XtPointer client_data, XtPointer) {
        auto *owner = static_cast<native::app_wnd *>(client_data);
        if (owner)
            owner->destroy();
    }
} // namespace

namespace native
{
    void app_wnd::apply_title() {
        Widget shell =
            linux::openmotif::shell_bindings.handle_from_object(this);
        if (!shell)
            throw std::runtime_error(
                "Motif: Missing shell binding for app_wnd.");

        XtVaSetValues(shell, XtNtitle, _title.c_str(), nullptr);
    }

    void app_wnd::create() const {
        if (_created)
            return;

        Widget shell = nullptr;
        Display *probe_display = linux::openmotif::cached_display;

        if (!linux::openmotif::app_instance) {
            int argc = app::argc;
            char **argv = app::argv;

            shell = XtVaAppInitialize(
                &linux::openmotif::app_instance,
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

            linux::openmotif::cached_display = XtDisplay(shell);
        }
        else {
            shell = XtVaAppCreateShell(
                const_cast<char *>("native"),
                const_cast<char *>("Native"),
                applicationShellWidgetClass,
                linux::openmotif::cached_display,
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

        // XmMainWindow sits between shell and canvas so the menu bar
        // can be attached via XmNmenuBar without violating the one-child
        // rule of the shell widget.
        Widget main_win = XmCreateMainWindow(shell, const_cast<char *>("main_window"), nullptr, 0);
        XtManageChild(main_win);

        Widget canvas = XmCreateDrawingArea(main_win, const_cast<char *>("canvas"), nullptr, 0);
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
            ExposureMask |
                StructureNotifyMask |
                PointerMotionMask |
                ButtonPressMask |
                ButtonReleaseMask,
            False,
            handle_canvas_event,
            const_cast<app_wnd *>(this));
        XtAddEventHandler(
            shell,
            StructureNotifyMask,
            False,
            handle_shell_event,
            const_cast<app_wnd *>(this));

        linux::openmotif::shell_bindings.register_pair(shell, const_cast<app_wnd *>(this));
        linux::openmotif::main_wnd_bindings.register_pair(main_win, const_cast<app_wnd *>(this));
        linux::openmotif::wnd_bindings.register_pair(canvas, const_cast<app_wnd *>(this));

        _created = true;
        const_cast<app_wnd *>(this)->menu.attach(*const_cast<app_wnd *>(this));
        const_cast<app_wnd *>(this)->on_wnd_create.emit();
    }

    void app_wnd::show() const {
        if (!_created)
            throw std::runtime_error("Motif: Cannot show window before it is created.");

        app_wnd *self = const_cast<app_wnd *>(this);
        Widget shell = linux::openmotif::shell_bindings.handle_from_object(self);
        Widget canvas = linux::openmotif::wnd_bindings.handle_from_object(self);
        if (!shell)
            throw std::runtime_error("Motif: Missing shell binding for app_wnd.");
        if (!canvas)
            throw std::runtime_error("Motif: Missing drawing widget binding for app_wnd.");

        XtRealizeWidget(shell);
        linux::openmotif::cached_display = XtDisplay(shell);

        // Prevent XClearArea from blanking the drawing surface before Expose.
        // The backbuffer XCopyArea should own what reaches the screen.
        XSetWindowBackgroundPixmap(linux::openmotif::cached_display, XtWindow(canvas), None);

        linux::openmotif::wm_delete_window_atom = XmInternAtom(
            linux::openmotif::cached_display,
            const_cast<char *>("WM_DELETE_WINDOW"),
            False);
        XmAddWMProtocolCallback(
            shell,
            linux::openmotif::wm_delete_window_atom,
            handle_wm_delete,
            self);
        XmActivateWMProtocol(shell, linux::openmotif::wm_delete_window_atom);

        XMapRaised(linux::openmotif::cached_display, XtWindow(shell));
        XFlush(linux::openmotif::cached_display);
        invalidate();
    }

    void app_wnd::destroy() const {
        if (!_created)
            return;

        app_wnd *self = const_cast<app_wnd *>(this);
        Widget shell = linux::openmotif::shell_bindings.handle_from_object(self);
        self->on_native_destroy();

        linux::openmotif::wnd_bindings.unregister_by_object(self);
        linux::openmotif::main_wnd_bindings.unregister_by_object(self);
        linux::openmotif::shell_bindings.unregister_by_object(self);

        if (shell)
            XtDestroyWidget(shell);
        if (self == app::main_wnd())
            linux::openmotif::exit_requested = true;
    }
} // namespace native
