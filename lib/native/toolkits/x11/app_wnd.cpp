//
// Implements the X11 application window with Xt and Athena containers.
// The Athena form remains a drawable surface while also parenting
// native child controls.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>
#include <string>

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>

#include <native.h>
#include <native/app_wnd.h>

#include "globals.h"
#include "window_position.h"

namespace
{
    // Publish both the ICCCM and EWMH title properties. Some modern
    // window managers prefer _NET_WM_NAME and do not display Xt's
    // legacy WM_NAME value by itself.
    void apply_shell_title(Widget shell, const std::string &title) {
        XtVaSetValues(shell,
                      XtNtitle,
                      title.c_str(),
                      XtNiconName,
                      title.c_str(),
                      nullptr);

        if (!XtIsRealized(shell))
            return;

        Display *display = XtDisplay(shell);
        Window window = XtWindow(shell);
        XStoreName(display, window, title.c_str());
        XSetIconName(display, window, title.c_str());

        Atom utf8_string = XInternAtom(display, "UTF8_STRING", False);
        Atom net_wm_name = XInternAtom(display, "_NET_WM_NAME", False);
        Atom net_wm_icon_name =
            XInternAtom(display, "_NET_WM_ICON_NAME", False);
        const auto *title_bytes =
            reinterpret_cast<const unsigned char *>(title.data());
        const int title_length = static_cast<int>(title.size());

        XChangeProperty(display,
                        window,
                        net_wm_name,
                        utf8_string,
                        8,
                        PropModeReplace,
                        title_bytes,
                        title_length);
        XChangeProperty(display,
                        window,
                        net_wm_icon_name,
                        utf8_string,
                        8,
                        PropModeReplace,
                        title_bytes,
                        title_length);
    }

    native::mouse_button decode_button(unsigned int button) {
        switch (button) {
        case Button1:
            return native::mouse_button::left;
        case Button2:
            return native::mouse_button::middle;
        case Button3:
            return native::mouse_button::right;
        default:
            return native::mouse_button::none;
        }
    }

    void ensure_backbuffer(native::wnd *owner,
                           Widget canvas,
                           int width,
                           int height) {
        if (!owner || !canvas || !linux::x11::cached_display ||
            width <= 0 || height <= 0)
            return;

        auto *cache =
            linux::x11::wnd_gpx_bindings.object_from_handle(owner);
        if (!cache)
            return;

        if (cache->backbuffer && cache->buf_w == width &&
            cache->buf_h == height)
            return;

        if (cache->backbuffer) {
            XFreePixmap(linux::x11::cached_display, cache->backbuffer);
        }

        cache->backbuffer =
            XCreatePixmap(linux::x11::cached_display,
                          XtWindow(canvas),
                          static_cast<unsigned int>(width),
                          static_cast<unsigned int>(height),
                          DefaultDepthOfScreen(XtScreen(canvas)));
        cache->buf_w = width;
        cache->buf_h = height;
    }

    void handle_canvas_event(Widget widget,
                             XtPointer client_data,
                             XEvent *event,
                             Boolean *) {
        auto *owner = static_cast<native::app_wnd *>(client_data);
        if (!owner || !event)
            return;

        switch (event->type) {
        case Expose: {
            if (event->xexpose.count != 0)
                return;

            auto &g = owner->get_gpx();
            auto *cache =
                linux::x11::wnd_gpx_bindings.object_from_handle(owner);

            int width = cache ? cache->buf_w : 0;
            int height = cache ? cache->buf_h : 0;
            if (width <= 0 || height <= 0) {
                Dimension widget_width = 0;
                Dimension widget_height = 0;
                XtVaGetValues(widget,
                              XtNwidth,
                              &widget_width,
                              XtNheight,
                              &widget_height,
                              nullptr);
                width = static_cast<int>(widget_width);
                height = static_cast<int>(widget_height);
            }

            native::rect invalid(0,
                                 0,
                                 static_cast<native::dim>(width),
                                 static_cast<native::dim>(height));
            g.set_clip(invalid);
            g.clear(g.get_paper());

            native::wnd_paint_event paint_event(invalid, g);
            owner->on_wnd_paint.emit(paint_event);

            cache =
                linux::x11::wnd_gpx_bindings.object_from_handle(owner);
            if (cache && cache->gc && cache->backbuffer) {
                // Drawing operations retain their last logical clip in
                // the shared GC. Presentation must copy the complete
                // backbuffer, independently of that drawing state.
                XSetClipMask(linux::x11::cached_display,
                             cache->gc,
                             None);
                XCopyArea(linux::x11::cached_display,
                          cache->backbuffer,
                          XtWindow(widget),
                          cache->gc,
                          0,
                          0,
                          static_cast<unsigned int>(cache->buf_w),
                          static_cast<unsigned int>(cache->buf_h),
                          0,
                          0);
                XFlush(linux::x11::cached_display);
            }
            break;
        }

        case ConfigureNotify: {
            const int width = event->xconfigure.width;
            const int height = event->xconfigure.height;
            ensure_backbuffer(owner, widget, width, height);

            native::size dimensions(static_cast<native::dim>(width),
                                    static_cast<native::dim>(height));
            owner->on_native_resize(dimensions);
            owner->on_wnd_resize.emit(dimensions);
            owner->invalidate();
            break;
        }

        case MotionNotify:
            if (!owner->get_input_enabled())
                return;
            owner->on_mouse_move.emit(
                native::point(event->xmotion.x, event->xmotion.y));
            break;

        case ButtonPress:
        case ButtonRelease: {
            if (!owner->get_input_enabled())
                return;
            if (event->xbutton.button == Button4 ||
                event->xbutton.button == Button5) {
                owner->on_mouse_wheel.emit(native::mouse_wheel_event(
                    native::point(event->xbutton.x, event->xbutton.y),
                    static_cast<native::coord>(
                        event->xbutton.button == Button4 ? 1 : -1),
                    native::wheel_direction::vertical));
                return;
            }

            native::mouse_button button =
                decode_button(event->xbutton.button);
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

    // Move a shell only when its preferred frame would be unreachable.
    void keep_shell_reachable(native::app_wnd *owner, Widget shell) {
        const native::point current = owner->get_position();
        const native::point position =
            linux::x11::constrain_shell_position(
                shell, current, owner->get_dimensions());
        if (position.x == current.x && position.y == current.y)
            return;

        XtVaSetValues(shell,
                      XtNx,
                      position.x,
                      XtNy,
                      position.y,
                      nullptr);
        owner->on_native_move(position);
    }

    void handle_shell_event(Widget widget,
                            XtPointer client_data,
                            XEvent *event,
                            Boolean *) {
        auto *owner = static_cast<native::app_wnd *>(client_data);
        if (!owner || !event)
            return;

        if (event->type == ConfigureNotify) {
            Position root_x = event->xconfigure.x;
            Position root_y = event->xconfigure.y;
            if (XtIsRealized(widget)) {
                XtTranslateCoords(
                    widget, 0, 0, &root_x, &root_y);
            }
            native::point position(root_x, root_y);
            owner->on_native_move(position);
            owner->on_wnd_move.emit(position);
            keep_shell_reachable(owner, widget);
        } else if (event->type == MapNotify) {
            keep_shell_reachable(owner, widget);
            // A transient shell is not focusable until the server has
            // made it viewable. Focusing earlier raises BadMatch under
            // asynchronous window managers.
            if (owner->get_modal()) {
                XSetInputFocus(event->xmap.display,
                               event->xmap.window,
                               RevertToParent,
                               CurrentTime);
            }
        } else if (event->type == PropertyNotify &&
                   event->xproperty.atom == XInternAtom(
                       event->xproperty.display,
                       "_NET_FRAME_EXTENTS",
                       True)) {
            keep_shell_reachable(owner, widget);
        } else if (event->type == ClientMessage &&
                   event->xclient.data.l[0] ==
                       static_cast<long>(
                           linux::x11::wm_delete_window_atom)) {
            owner->destroy();
        }
    }
} // namespace

namespace native
{
    void app_wnd::apply_title() {
        Widget shell =
            linux::x11::shell_bindings.handle_from_object(this);
        if (!shell)
            throw std::runtime_error(
                "X11/Athena: Missing shell binding for app_wnd.");

        apply_shell_title(shell, _title);
        if (XtIsRealized(shell))
            XFlush(XtDisplay(shell));
    }

    void app_wnd::create() const {
        if (_created)
            return;

        validate_owner_created();
        Widget shell = nullptr;
        Display *probe_display = linux::x11::cached_display;

        if (!linux::x11::app_instance) {
            XtSetLanguageProc(nullptr, nullptr, nullptr);

            int argc = app::argc;
            char **argv = app::argv;

            shell = XtVaAppInitialize(&linux::x11::app_instance,
                                      const_cast<char *>("Native"),
                                      nullptr,
                                      0,
                                      &argc,
                                      argv,
                                      nullptr,
                                      nullptr);
            if (!shell)
                throw std::runtime_error(
                    "X11/Athena: Failed to initialize Xt shell.");

            if (probe_display && probe_display != XtDisplay(shell))
                XCloseDisplay(probe_display);
            linux::x11::cached_display = XtDisplay(shell);
        } else if (app_wnd *owner = get_owner()) {
            Widget owner_shell =
                linux::x11::shell_bindings.handle_from_object(owner);
            if (get_modal()) {
                shell = XtVaCreatePopupShell(
                    const_cast<char *>("native"),
                    transientShellWidgetClass,
                    owner_shell,
                    XtNtransientFor,
                    owner_shell,
                    nullptr);
            } else {
                // ICCCM transient and window-group hints both permit a
                // window manager to keep an owned shell above its
                // leader. Modeless ownership is therefore maintained by
                // the portable owner graph, while the native shell stays
                // an independently stackable top-level window.
                shell = XtVaAppCreateShell(
                    const_cast<char *>("native"),
                    const_cast<char *>("Native"),
                    topLevelShellWidgetClass,
                    linux::x11::cached_display,
                    nullptr);
            }
            if (!shell)
                throw std::runtime_error(
                    "X11/Athena: Failed to create owned shell.");
        } else {
            shell = XtVaAppCreateShell(const_cast<char *>("native"),
                                       const_cast<char *>("Native"),
                                       applicationShellWidgetClass,
                                       linux::x11::cached_display,
                                       nullptr);
            if (!shell)
                throw std::runtime_error(
                    "X11/Athena: Failed to create top-level shell.");
        }

        const point position =
            linux::x11::constrain_shell_position(
                shell, _bounds.p, _bounds.d);
        XtVaSetValues(shell,
                      XtNx,
                      position.x,
                      XtNy,
                      position.y,
                      XtNwidth,
                      _bounds.d.w,
                      XtNheight,
                      _bounds.d.h,
                      XtNtitle,
                      _title.c_str(),
                      nullptr);

        Widget main_window = XtVaCreateManagedWidget("main_window",
                                                     formWidgetClass,
                                                     shell,
                                                     XtNwidth,
                                                     _bounds.d.w,
                                                     XtNheight,
                                                     _bounds.d.h,
                                                     XtNborderWidth,
                                                     0,
                                                     XtNdefaultDistance,
                                                     0,
                                                     nullptr);
        if (!main_window)
            throw std::runtime_error(
                "X11/Athena: Failed to create main container.");

        auto *self = const_cast<app_wnd *>(this);
        linux::x11::shell_bindings.register_pair(shell, self);
        linux::x11::main_wnd_bindings.register_pair(main_window, self);

        // Create the menu first so the Form can anchor the drawing
        // surface immediately below the Athena menu bar.
        self->menu.attach(*self);

        Widget menu_bar = nullptr;
        if (self->menu.id()) {
            auto *native_menu =
                linux::x11::menu_bindings.object_from_handle(
                    self->menu.id());
            if (native_menu)
                menu_bar = native_menu->menu_bar;
        }

        Dimension menu_height = 0;
        if (menu_bar) {
            XtVaGetValues(menu_bar, XtNheight, &menu_height, nullptr);
        }

        const int canvas_height =
            _bounds.d.h > menu_height
                ? static_cast<int>(_bounds.d.h - menu_height)
                : 1;

        Widget canvas = nullptr;
        if (menu_bar) {
            canvas = XtVaCreateManagedWidget("canvas",
                                             formWidgetClass,
                                             main_window,
                                             XtNfromVert,
                                             menu_bar,
                                             XtNvertDistance,
                                             0,
                                             XtNhorizDistance,
                                             0,
                                             XtNwidth,
                                             _bounds.d.w,
                                             XtNheight,
                                             canvas_height,
                                             XtNborderWidth,
                                             0,
                                             XtNdefaultDistance,
                                             0,
                                             XtNleft,
                                             XtChainLeft,
                                             XtNright,
                                             XtChainRight,
                                             XtNtop,
                                             XtChainTop,
                                             XtNbottom,
                                             XtChainBottom,
                                             XtNresizable,
                                             False,
                                             nullptr);
        } else {
            canvas = XtVaCreateManagedWidget("canvas",
                                             formWidgetClass,
                                             main_window,
                                             XtNhorizDistance,
                                             0,
                                             XtNvertDistance,
                                             0,
                                             XtNwidth,
                                             _bounds.d.w,
                                             XtNheight,
                                             _bounds.d.h,
                                             XtNborderWidth,
                                             0,
                                             XtNdefaultDistance,
                                             0,
                                             XtNleft,
                                             XtChainLeft,
                                             XtNright,
                                             XtChainRight,
                                             XtNtop,
                                             XtChainTop,
                                             XtNbottom,
                                             XtChainBottom,
                                             XtNresizable,
                                             False,
                                             nullptr);
        }
        if (!canvas)
            throw std::runtime_error(
                "X11/Athena: Failed to create drawing surface.");

        XtAddEventHandler(canvas,
                          ExposureMask | StructureNotifyMask |
                              PointerMotionMask | ButtonPressMask |
                              ButtonReleaseMask,
                          False,
                          handle_canvas_event,
                          self);
        XtAddEventHandler(shell,
                          StructureNotifyMask | PropertyChangeMask,
                          True,
                          handle_shell_event,
                          self);

        linux::x11::wnd_bindings.register_pair(canvas, self);

        _created = true;
        self->on_wnd_create.emit();
    }

    void app_wnd::show() const {
        if (!_created)
            throw std::runtime_error(
                "X11/Athena: Cannot show window before creation.");

        auto *self = const_cast<app_wnd *>(this);
        Widget shell =
            linux::x11::shell_bindings.handle_from_object(self);
        Widget canvas =
            linux::x11::wnd_bindings.handle_from_object(self);
        if (!shell || !canvas)
            throw std::runtime_error(
                "X11/Athena: Missing application widget binding.");

        XtRealizeWidget(shell);
        linux::x11::cached_display = XtDisplay(shell);
        apply_shell_title(shell, _title);
        linux::x11::request_frame_extents(shell);
        keep_shell_reachable(self, shell);

        // Expose notifications repaint from the library backbuffer.
        XSetWindowBackgroundPixmap(
            linux::x11::cached_display, XtWindow(canvas), None);

        linux::x11::wm_delete_window_atom = XInternAtom(
            linux::x11::cached_display, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(linux::x11::cached_display,
                        XtWindow(shell),
                        &linux::x11::wm_delete_window_atom,
                        1);

        if (get_owner())
            XtPopup(shell,
                    get_modal() ? XtGrabExclusive : XtGrabNone);
        else
            XMapRaised(linux::x11::cached_display, XtWindow(shell));
        XFlush(linux::x11::cached_display);
        invalidate();
    }

    void app_wnd::destroy() const {
        if (!_created)
            return;

        auto *self = const_cast<app_wnd *>(this);
        Widget shell =
            linux::x11::shell_bindings.handle_from_object(self);
        app_wnd *owner = get_owner();
        self->on_native_destroy();

        linux::x11::wnd_bindings.unregister_by_object(self);
        linux::x11::main_wnd_bindings.unregister_by_object(self);
        linux::x11::shell_bindings.unregister_by_object(self);

        if (shell) {
            if (get_owner())
                XtPopdown(shell);
            XtDestroyWidget(shell);
        }
        if (get_modal() && owner) {
            app_wnd *focus = owner->get_input_enabled()
                                 ? owner
                                 : owner->get_active_modal();
            Widget focus_shell =
                focus ? linux::x11::shell_bindings
                            .handle_from_object(focus)
                      : nullptr;
            if (focus_shell && XtIsRealized(focus_shell)) {
                XSetInputFocus(linux::x11::cached_display,
                               XtWindow(focus_shell),
                               RevertToParent,
                               CurrentTime);
            }
        }
        if (self == app::main_wnd())
            linux::x11::exit_requested = true;
    }
} // namespace native
