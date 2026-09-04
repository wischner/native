//
// Implements the X11 structural container with an Athena Form. The
// Form is a real Xt composite, so every Native control can name it as
// its parent widget, and the X server paints its background.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>

#include <native.h>
#include <native/panel.h>

#include "globals.h"

namespace
{
    // Route only the notifications a structural container needs. Keys
    // and focus belong to the children, and the server repaints the
    // Form background, so no expose handler is installed.
    void route_event(Widget,
                     XtPointer client_data,
                     XEvent *event,
                     Boolean *) {
        auto *owner = static_cast<native::panel *>(client_data);
        if (!owner || !event)
            return;

        switch (event->type) {
        case ConfigureNotify:
            owner->on_native_resize(native::size(
                static_cast<native::dim>(event->xconfigure.width),
                static_cast<native::dim>(event->xconfigure.height)));
            break;
        case MotionNotify:
            owner->on_native_mouse_move(
                native::point(event->xmotion.x, event->xmotion.y));
            break;
        case ButtonPress:
        case ButtonRelease: {
            if (event->xbutton.button == Button4 ||
                event->xbutton.button == Button5) {
                owner->on_native_mouse_wheel(
                    native::mouse_wheel_event(
                        native::point(event->xbutton.x,
                                      event->xbutton.y),
                        static_cast<native::coord>(
                            event->xbutton.button == Button4 ? 24 : -24),
                        native::wheel_direction::vertical));
                break;
            }
            if (event->xbutton.button != Button1)
                break;
            owner->on_native_mouse_click(native::mouse_event(
                native::mouse_button::left,
                event->type == ButtonPress
                    ? native::mouse_action::press
                    : native::mouse_action::release,
                native::point(event->xbutton.x, event->xbutton.y)));
            break;
        }
        }
    }
} // namespace

namespace native
{
    void panel::create_native() {
        wnd *parent = get_parent();
        if (!parent)
            throw std::runtime_error(
                "X11/Athena: panel requires a parent window.");
        if (!parent->get_created())
            throw std::runtime_error(
                "X11/Athena: panel parent is not created.");

        Widget parent_widget =
            linux::x11::wnd_bindings.handle_from_object(parent);
        if (!parent_widget)
            throw std::runtime_error(
                "X11/Athena: panel parent has no widget.");

        auto *self = this;
        Widget widget = XtVaCreateWidget(
            "panel",
            formWidgetClass,
            parent_widget,
            XtNhorizDistance,
            _bounds.p.x,
            XtNvertDistance,
            _bounds.p.y,
            XtNwidth,
            linux::x11::widget_dimension(_bounds.d.w),
            XtNheight,
            linux::x11::widget_dimension(_bounds.d.h),
            XtNborderWidth,
            0,
            // A structural host follows the geometry its layout gives
            // it. Chaining every edge to the top-left stops Form from
            // renegotiating the panel's own size when a child changes.
            XtNleft,
            XtChainLeft,
            XtNright,
            XtChainLeft,
            XtNtop,
            XtChainTop,
            XtNbottom,
            XtChainTop,
            XtNresizable,
            True,
            nullptr);
        if (!widget)
            throw std::runtime_error(
                "X11/Athena: failed to create panel host.");

        XtAddEventHandler(widget,
                          StructureNotifyMask | ButtonPressMask |
                              ButtonReleaseMask | PointerMotionMask,
                          False,
                          route_event,
                          self);

        // Children resolve their Xt parent through this registry, so
        // the binding has to exist before on_wnd_create runs.
        linux::x11::wnd_bindings.register_pair(widget, self);
    }

    void panel::show_native() {
        Widget widget = linux::x11::wnd_bindings.handle_from_object(
            this);
        if (!_created || !widget)
            throw std::runtime_error(
                "X11/Athena: panel is not created.");
        XtManageChild(widget);
    }

    void panel::destroy_native() {
        if (!_created)
            return;

        auto *self = this;
        Widget widget =
            linux::x11::wnd_bindings.handle_from_object(self);
        if (widget) {
            linux::x11::wnd_bindings.unregister_by_handle(widget);
            XtDestroyWidget(widget);
        }
    }
} // namespace native
