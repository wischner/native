//
// Implements the OPEN LOOK structural container and paintable child
// surface. Both are XView Panels under the top-level Frame, which is
// where this backend places every child region.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <native.h>
#include <native/canvas.h>
#include <native/panel.h>

#include <xview/panel.h>
#include <xview/xview.h>

#include "collection_host.h"
#include "globals.h"

namespace
{
    //
    // Return a child region's position inside its top-level Frame.
    //
    // Notes:
    //      XView places every Native child region directly on the
    //      Frame, so nested panel offsets are accumulated here rather
    //      than delegated to a real widget hierarchy.
    //
    native::point frame_position(native::wnd &owner,
                                 native::app_wnd *&window) {
        native::point position = owner.get_position();
        native::wnd *root = owner.get_parent();
        while (root && !dynamic_cast<native::app_wnd *>(root)) {
            position.x = static_cast<native::coord>(
                position.x + root->get_position().x);
            position.y = static_cast<native::coord>(
                position.y + root->get_position().y);
            root = root->get_parent();
        }
        window = dynamic_cast<native::app_wnd *>(root);
        return position;
    }
} // namespace

namespace native
{
    void panel::create() const {
        if (_created)
            return;

        auto *self = const_cast<panel *>(this);
        app_wnd *window = nullptr;
        const point position = frame_position(*self, window);
        auto *window_state =
            window ? linux::openlook::window_state(window) : nullptr;
        if (!window_state || !window_state->frame)
            throw std::runtime_error(
                "OpenLook/XView: panel requires a created top-level "
                "parent.");

        // A bare Panel with no border and no repaint procedure is the
        // toolkit's own empty container: XView clears it to its
        // background, so exposed panel space never shows stale pixels.
        Panel host = static_cast<Panel>(
            xv_create(window_state->frame,
                      PANEL,
                      PANEL_LAYOUT,
                      PANEL_HORIZONTAL,
                      PANEL_BORDER,
                      FALSE,
                      XV_X,
                      position.x,
                      XV_Y,
                      position.y + window_state->menu_height,
                      XV_WIDTH,
                      _bounds.d.w,
                      XV_HEIGHT,
                      _bounds.d.h,
                      XV_SHOW,
                      FALSE,
                      nullptr));
        if (!host)
            throw std::runtime_error(
                "OpenLook/XView: failed to create panel.");

        // Children resolve their parent Panel through this registry,
        // so the binding has to exist before on_wnd_create runs.
        linux::openlook::wnd_bindings.register_pair(host, self);
        _created = true;
        self->on_native_create();
    }

    void panel::show() const {
        auto host = static_cast<Panel>(
            linux::openlook::wnd_bindings.handle_from_object(
                const_cast<panel *>(this)));
        if (!_created || !host)
            throw std::runtime_error(
                "OpenLook/XView: panel is not created.");
        xv_set(host, XV_SHOW, TRUE, nullptr);
    }

    void panel::destroy() const {
        if (!_created)
            return;

        auto *self = const_cast<panel *>(this);
        auto host = static_cast<Panel>(
            linux::openlook::wnd_bindings.handle_from_object(self));
        self->on_native_destroy();
        if (host) {
            linux::openlook::wnd_bindings.unregister_by_handle(host);
            xv_destroy_safe(host);
        }
    }

    void canvas::create() const {
        if (_created)
            return;

        auto *self = const_cast<canvas *>(this);
        auto *state = linux::openlook::create_collection_panel(*self);
        linux::openlook::canvas_bindings.register_pair(self, state);
        _created = true;
        self->synchronize_theme_metrics();
        self->relayout_children();
        self->on_native_create();
    }

    void canvas::show() const {
        auto *state = linux::openlook::canvas_bindings
                          .object_from_handle(
                              const_cast<canvas *>(this));
        if (!_created || !state || !state->panel)
            throw std::runtime_error(
                "OpenLook/XView: canvas is not created.");
        xv_set(state->panel, XV_SHOW, TRUE, nullptr);
        const Window window =
            static_cast<Window>(xv_get(state->panel, XV_XID));
        if (window != None && linux::openlook::cached_display) {
            XRaiseWindow(linux::openlook::cached_display, window);
            XFlush(linux::openlook::cached_display);
        }
    }

    void canvas::destroy() const {
        if (!_created)
            return;

        auto *self = const_cast<canvas *>(this);
        auto *state =
            linux::openlook::canvas_bindings.object_from_handle(self);
        linux::openlook::destroy_collection_panel(*self, state);
        linux::openlook::canvas_bindings.unregister_by_handle(self);
    }
} // namespace native
