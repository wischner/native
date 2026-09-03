//
// Implements the Window Maker structural container and paintable child
// surface. The panel is a flat WINGs frame usable as a parent widget;
// the canvas reuses the shared collection frame and its routing.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <WINGs/WINGs.h>
#include <WINGs/WINGsP.h>

#include <native.h>
#include <native/canvas.h>
#include <native/panel.h>

#include "collection_host.h"
#include "globals.h"

namespace
{
    // Route only what a structural container needs. WINGs repaints the
    // frame background, so no expose handler is installed.
    void route_panel_event(XEvent *event, void *data) {
        auto *owner = static_cast<native::panel *>(data);
        if (!owner || !event || !owner->get_created())
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
        WMWidget *parent_host = linux::wmaker::parent_widget(self);
        if (!parent || !parent->get_created() || !parent_host)
            throw std::runtime_error(
                "Window Maker/WINGs: panel requires a created parent.");

        WMFrame *frame = WMCreateFrame(parent_host);
        if (!frame)
            throw std::runtime_error(
                "Window Maker/WINGs: failed to create panel frame.");

        const point position = linux::wmaker::control_position(self);
        WMMoveWidget(frame, position.x, position.y);
        WMResizeWidget(frame, _bounds.d.w, _bounds.d.h);
        // A structural container is visually empty.
        WMSetFrameRelief(frame, WRFlat);

        // Children resolve their parent widget through this registry,
        // so the binding has to exist before on_wnd_create runs.
        linux::wmaker::wnd_bindings.register_pair(frame, self);
        WMCreateEventHandler(WMWidgetView(frame),
                             StructureNotifyMask | ButtonPressMask |
                                 ButtonReleaseMask | PointerMotionMask,
                             route_panel_event,
                             self);
        _created = true;
        self->on_native_create();
    }

    void panel::show() const {
        auto *frame = static_cast<WMFrame *>(
            linux::wmaker::wnd_bindings.handle_from_object(
                const_cast<panel *>(this)));
        if (!_created || !frame)
            throw std::runtime_error(
                "Window Maker/WINGs: panel is not created.");
        // A panel can also be created after its parent was realized,
        // and WINGs does not realize such descendants on mapping.
        WMRealizeWidget(frame);
        WMMapWidget(frame);
    }

    void panel::destroy() const {
        if (!_created)
            return;

        auto *self = const_cast<panel *>(this);
        auto *frame = static_cast<WMFrame *>(
            linux::wmaker::wnd_bindings.handle_from_object(self));
        self->on_native_destroy();
        linux::wmaker::wnd_bindings.unregister_by_object(self);
        if (frame)
            WMDestroyWidget(frame);
    }

    void canvas::create() const {
        if (_created)
            return;

        auto *self = const_cast<canvas *>(this);
        auto *state = linux::wmaker::create_collection_frame(*self);
        linux::wmaker::canvas_bindings.register_pair(self, state);
        _created = true;
        self->synchronize_theme_metrics();
        self->relayout_children();
        self->on_native_create();
    }

    void canvas::show() const {
        auto *state = linux::wmaker::canvas_bindings.object_from_handle(
            const_cast<canvas *>(this));
        if (!_created || !state || !state->frame)
            throw std::runtime_error(
                "Window Maker/WINGs: canvas is not created.");
        WMRealizeWidget(state->frame);
        WMMapWidget(state->frame);
        WMRaiseWidget(state->frame);
    }

    void canvas::destroy() const {
        if (!_created)
            return;

        auto *self = const_cast<canvas *>(this);
        auto *state =
            linux::wmaker::canvas_bindings.object_from_handle(self);
        linux::wmaker::destroy_collection_frame(*self, state);
        linux::wmaker::canvas_bindings.unregister_by_handle(self);
    }
} // namespace native
