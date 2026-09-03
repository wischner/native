//
// Implements the Motif structural container and paintable child
// surface. The panel is an XmForm so every Native control can name it
// as its Xt parent; the canvas reuses the shared drawing-area host.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <Xm/Form.h>
#include <Xm/Xm.h>

#include <native.h>
#include <native/canvas.h>
#include <native/panel.h>

#include "collection_host.h"
#include "globals.h"

namespace
{
    // Route only what a structural container needs. Motif repaints the
    // Form background, so no expose handler is installed.
    void route_panel_event(Widget,
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
    void panel::create() const {
        if (_created)
            return;

        auto *self = const_cast<panel *>(this);
        wnd *parent = get_parent();
        Widget parent_host = linux::openmotif::parent_widget(self);
        if (!parent || !parent->get_created() || !parent_host)
            throw std::runtime_error(
                "Motif: panel requires a created parent.");

        Widget widget = XtVaCreateWidget("panel",
                                         xmFormWidgetClass,
                                         parent_host,
                                         XmNx,
                                         _bounds.p.x,
                                         XmNy,
                                         _bounds.p.y,
                                         XmNwidth,
                                         _bounds.d.w,
                                         XmNheight,
                                         _bounds.d.h,
                                         XmNborderWidth,
                                         0,
                                         // A structural host takes the
                                         // geometry its layout gives it
                                         // and never renegotiates when
                                         // a child changes size.
                                         XmNresizePolicy,
                                         XmRESIZE_NONE,
                                         nullptr);
        if (!widget)
            throw std::runtime_error(
                "Motif: failed to create panel form.");

        XtAddEventHandler(widget,
                          StructureNotifyMask | ButtonPressMask |
                              ButtonReleaseMask | PointerMotionMask,
                          False,
                          route_panel_event,
                          self);

        // Children resolve their Xt parent through this registry, so
        // the binding has to exist before on_wnd_create runs.
        linux::openmotif::wnd_bindings.register_pair(widget, self);
        _created = true;
        self->on_native_create();
    }

    void panel::show() const {
        Widget widget =
            linux::openmotif::wnd_bindings.handle_from_object(
                const_cast<panel *>(this));
        if (!_created || !widget)
            throw std::runtime_error("Motif: panel is not created.");
        XtManageChild(widget);
    }

    void panel::destroy() const {
        if (!_created)
            return;

        auto *self = const_cast<panel *>(this);
        Widget widget =
            linux::openmotif::wnd_bindings.handle_from_object(self);
        self->on_native_destroy();
        if (widget) {
            linux::openmotif::wnd_bindings.unregister_by_handle(widget);
            XtDestroyWidget(widget);
        }
    }

    void canvas::create() const {
        if (_created)
            return;

        auto *self = const_cast<canvas *>(this);
        auto *state = new linux::openmotif::motif_collection();
        state->widget =
            linux::openmotif::create_collection_host(*self, *state);
        linux::openmotif::canvas_bindings.register_pair(self, state);
        _created = true;
        self->synchronize_theme_metrics();
        self->relayout_children();
        self->on_native_create();
    }

    void canvas::show() const {
        auto *state = linux::openmotif::canvas_bindings
                          .object_from_handle(
                              const_cast<canvas *>(this));
        if (!_created || !state || !state->widget)
            throw std::runtime_error("Motif: canvas is not created.");
        XtManageChild(state->widget);
        if (XtIsRealized(state->widget)) {
            XRaiseWindow(linux::openmotif::cached_display,
                         XtWindow(state->widget));
        }
    }

    void canvas::destroy() const {
        if (!_created)
            return;

        auto *self = const_cast<canvas *>(this);
        auto *state =
            linux::openmotif::canvas_bindings.object_from_handle(self);
        linux::openmotif::destroy_collection_host(*self, state);
        linux::openmotif::canvas_bindings.unregister_by_handle(self);
    }
} // namespace native
