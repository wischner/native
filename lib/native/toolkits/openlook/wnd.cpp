//
// Implements portable window geometry, invalidation, and graphics
// access for XView frames and Panel controls.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <native.h>
#include <native/wnd.h>

#include <xview/panel.h>
#include <xview/xview.h>

#include "../../gpx_wnd.h"
#include "globals.h"
#include "window_position.h"

namespace
{
    Panel_item control_item(native::wnd *window) {
        Xv_opaque handle = linux::openlook::wnd_bindings
                               .handle_from_object(window);
        return static_cast<Panel_item>(handle);
    }
} // namespace

namespace native
{
    void wnd::apply_position() {
        if (auto *window = dynamic_cast<app_wnd *>(this)) {
            auto *state = linux::openlook::window_state(window);
            if (!state || !state->frame)
                return;
            const point position =
                linux::openlook::constrain_frame_position(
                    state->frame, _bounds.p, _bounds.d);
            xv_set(state->frame,
                   XV_X,
                   position.x,
                   XV_Y,
                   position.y,
                   nullptr);
            return;
        }

        Panel_item item = control_item(this);
        if (item) {
            xv_set(item,
                   XV_X,
                   _bounds.p.x,
                   XV_Y,
                   _bounds.p.y,
                   nullptr);
        }
    }

    void wnd::apply_dimensions() {
        if (auto *window = dynamic_cast<app_wnd *>(this)) {
            auto *state = linux::openlook::window_state(window);
            if (!state || !state->frame)
                return;
            xv_set(state->content,
                   XV_WIDTH,
                   _bounds.d.w,
                   XV_HEIGHT,
                   _bounds.d.h,
                   nullptr);
            xv_set(state->frame,
                   XV_WIDTH,
                   _bounds.d.w,
                   XV_HEIGHT,
                   _bounds.d.h + state->menu_height,
                   nullptr);
            apply_position();
            return;
        }

        Panel_item item = control_item(this);
        if (item) {
            xv_set(item,
                   XV_WIDTH,
                   _bounds.d.w,
                   XV_HEIGHT,
                   _bounds.d.h,
                   nullptr);
        }
    }

    void wnd::apply_bounds() {
        apply_dimensions();
        apply_position();
    }

    void wnd::apply_parent() {
        if (dynamic_cast<app_wnd *>(this))
            return;

        Panel_item item = control_item(this);
        const bool was_visible =
            item && static_cast<bool>(xv_get(item, XV_SHOW));
        destroy();
        if (_parent) {
            create();
            if (was_visible)
                show();
        }
    }

    wnd &wnd::invalidate() const {
        if (!_created)
            return const_cast<wnd &>(*this);

        auto *self = const_cast<wnd *>(this);
        if (auto *window = dynamic_cast<app_wnd *>(self)) {
            linux::openlook::repaint_window(
                window,
                rect(point(0, 0), window->get_dimensions()));
        } else if (Panel_item item = control_item(self)) {
            panel_paint(item, PANEL_CLEAR);
        }
        return *self;
    }

    wnd &wnd::invalidate(const rect &area) const {
        if (!_created)
            return const_cast<wnd &>(*this);

        auto *self = const_cast<wnd *>(this);
        if (auto *window = dynamic_cast<app_wnd *>(self)) {
            linux::openlook::repaint_window(window, area);
        } else if (Panel_item item = control_item(self)) {
            panel_paint(item, PANEL_CLEAR);
        }
        return *self;
    }

    gpx &wnd::get_gpx() const {
        if (!_created) {
            throw std::runtime_error(
                "Cannot obtain gpx before window is created.");
        }
        if (!_gpx)
            _gpx = new gpx_wnd(this);
        return *_gpx;
    }
} // namespace native
