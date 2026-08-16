//
// Implements portable geometry, parenting, invalidation, and graphics
// access for WINGs windows and controls.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <WINGs/WINGs.h>

#include <native/app_wnd.h>
#include <native/wnd.h>

#include "../../gpx_wnd.h"
#include "globals.h"

namespace native
{
    void wnd::apply_position() {
        if (auto *window = dynamic_cast<app_wnd *>(this)) {
            auto *window_state = linux::wmaker::state(window);
            if (!window_state || !window_state->window)
                return;
            const point position =
                linux::wmaker::constrain_position(_bounds.p, _bounds.d);
            WMSetWindowUserPosition(
                window_state->window, position.x, position.y);
            if (WMWidgetXID(window_state->window) != None) {
                XMoveWindow(linux::wmaker::display,
                            WMWidgetXID(window_state->window),
                            position.x,
                            position.y);
            }
            return;
        }

        WMWidget *widget =
            linux::wmaker::wnd_bindings.handle_from_object(this);
        if (widget) {
            const point position =
                linux::wmaker::control_position(this);
            WMMoveWidget(widget, position.x, position.y);
        }
    }

    void wnd::apply_dimensions() {
        WMWidget *widget =
            linux::wmaker::wnd_bindings.handle_from_object(this);
        if (!widget)
            return;
        int height = _bounds.d.h;
        if (auto *window = dynamic_cast<app_wnd *>(this)) {
            auto *window_state = linux::wmaker::state(window);
            height += window_state ? window_state->menu_height : 0;
        }
        WMResizeWidget(widget,
                       static_cast<unsigned int>(_bounds.d.w),
                       static_cast<unsigned int>(height));
    }

    void wnd::apply_bounds() {
        apply_dimensions();
        apply_position();
    }

    void wnd::apply_parent() {
        if (dynamic_cast<app_wnd *>(this))
            return;
        WMWidget *widget =
            linux::wmaker::wnd_bindings.handle_from_object(this);
        if (!widget)
            return;
        const point position =
            linux::wmaker::control_position(this);
        WMReparentWidget(widget,
                         linux::wmaker::parent_widget(this),
                         position.x,
                         position.y);
    }

    wnd &wnd::invalidate() const {
        if (!_created)
            return const_cast<wnd &>(*this);
        auto *self = const_cast<wnd *>(this);
        if (auto *window = dynamic_cast<app_wnd *>(self)) {
            linux::wmaker::schedule_repaint(
                window,
                rect({0, 0}, window->get_dimensions()));
        } else if (WMWidget *widget =
                       linux::wmaker::wnd_bindings
                           .handle_from_object(self)) {
            WMRedisplayWidget(widget);
        }
        return *self;
    }

    wnd &wnd::invalidate(const rect &area) const {
        if (!_created)
            return const_cast<wnd &>(*this);
        auto *self = const_cast<wnd *>(this);
        if (auto *window = dynamic_cast<app_wnd *>(self)) {
            linux::wmaker::schedule_repaint(window, area);
        } else if (WMWidget *widget =
                       linux::wmaker::wnd_bindings
                           .handle_from_object(self)) {
            WMRedisplayWidget(widget);
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
