//
// Implements the GEM structural panel and paintable canvas as nested
// regions of the emulated-control tree, together with their painting
// and pointer routing.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>
#include <vector>

#include <native.h>
#include <native/canvas.h>
#include <native/panel.h>

#include "../../gpx_wnd.h"
#include "globals.h"

namespace
{
    int depth_of(const native::wnd &control) {
        int depth = 0;
        for (native::wnd *parent = control.get_parent(); parent;
             parent = parent->get_parent())
            ++depth;
        return depth;
    }

    bool created_in(native::wnd *control, native::app_wnd *owner) {
        return control && control->get_created() &&
               linux::gemix::root_of(control) == owner;
    }

    // Return the deepest created region under a window-local point.
    template <typename control_type>
    control_type *region_at(const std::vector<control_type *> &registry,
                            native::app_wnd *owner,
                            native::point position) {
        control_type *found = nullptr;
        int best = -1;
        for (auto *control : registry) {
            if (!created_in(control, owner) ||
                !linux::gemix::root_bounds(*control).contains(position))
                continue;
            const int depth = depth_of(*control);
            if (depth > best) {
                best = depth;
                found = control;
            }
        }
        return found;
    }

    native::point local_point(native::wnd &control,
                              native::point position) {
        const native::point origin =
            linux::gemix::origin_in_root(control);
        return native::point(
            static_cast<native::coord>(position.x - origin.x),
            static_cast<native::coord>(position.y - origin.y));
    }
} // namespace

namespace linux::gemix
{
    native::wnd *root_of(native::wnd *control) {
        while (control && control->get_parent())
            control = control->get_parent();
        return control;
    }

    native::point origin_in_root(const native::wnd &control) {
        int x = control.get_position().x;
        int y = control.get_position().y;
        for (native::wnd *parent = control.get_parent();
             parent && parent->get_parent();
             parent = parent->get_parent()) {
            x += parent->get_position().x;
            y += parent->get_position().y;
        }
        return native::point(static_cast<native::coord>(x),
                             static_cast<native::coord>(y));
    }

    native::rect root_bounds(const native::wnd &control) {
        return native::rect(origin_in_root(control),
                            control.get_dimensions());
    }

    void render_surfaces(native::app_wnd *parent, native::gpx &g) {
        // Regions are painted parent first so a container never erases
        // the descendants drawn inside it.
        std::vector<native::wnd *> regions;
        for (auto *control : panels) {
            if (created_in(control, parent))
                regions.push_back(control);
        }
        for (auto *control : canvases) {
            if (created_in(control, parent))
                regions.push_back(control);
        }
        std::stable_sort(regions.begin(),
                         regions.end(),
                         [](native::wnd *left, native::wnd *right) {
                             return depth_of(*left) < depth_of(*right);
                         });

        auto painter = native::theme::create(g);
        for (native::wnd *region : regions) {
            const native::rect bounds = root_bounds(*region);
            if (!bounds.d.w || !bounds.d.h)
                continue;

            if (dynamic_cast<native::panel *>(region)) {
                // A panel is visually empty. Repainting the themed
                // container surface is what stops stale pixels from
                // showing where no child covers it.
                auto saved = g.save_state();
                g.set_clip(bounds);
                painter->draw_surface(bounds,
                                      native::surface_kind::panel,
                                      native::theme::state{});
                continue;
            }

            auto *surface = dynamic_cast<native::canvas *>(region);
            if (!surface)
                continue;

            // A canvas paints its client, rulers, and scrollbars in
            // canvas-local coordinates. A context carrying the
            // region's origin supplies that without translating the
            // window context every other control shares.
            native::gpx_wnd region_gpx(parent, bounds.p);
            const native::rect invalid(0,
                                       0,
                                       surface->get_dimensions().w,
                                       surface->get_dimensions().h);
            region_gpx.set_clip(invalid);
            native::wnd_paint_event event(invalid, region_gpx);
            surface->on_native_paint(event);
        }
    }

    bool dispatch_surface_click(native::app_wnd *parent,
                                native::point position,
                                bool pressed) {
        if (auto *surface = region_at(canvases, parent, position)) {
            surface->on_native_mouse_click(native::mouse_event(
                native::mouse_button::left,
                pressed ? native::mouse_action::press
                        : native::mouse_action::release,
                local_point(*surface, position)));
            return true;
        }
        // Nothing else claimed the position, so this is empty panel
        // space. The panel reports it and adds no action of its own.
        if (auto *host = region_at(panels, parent, position)) {
            host->on_native_mouse_click(native::mouse_event(
                native::mouse_button::left,
                pressed ? native::mouse_action::press
                        : native::mouse_action::release,
                local_point(*host, position)));
            return true;
        }
        return false;
    }

    bool dispatch_surface_move(native::app_wnd *parent,
                               native::point position) {
        if (auto *surface = region_at(canvases, parent, position)) {
            surface->on_native_mouse_move(
                local_point(*surface, position));
            return true;
        }
        if (auto *host = region_at(panels, parent, position)) {
            host->on_native_mouse_move(local_point(*host, position));
            return true;
        }
        return false;
    }
} // namespace linux::gemix

namespace native
{
    void panel::create() const {
        if (_created)
            return;

        wnd *parent = get_parent();
        if (!parent || !parent->get_created())
            throw std::runtime_error(
                "GEM: panel requires a created parent.");

        auto *self = const_cast<panel *>(this);
        linux::gemix::panels.push_back(self);
        _created = true;
        self->on_native_create();
    }

    void panel::show() const {
        if (!_created)
            throw std::runtime_error("GEM: panel is not created.");
        invalidate();
    }

    void panel::destroy() const {
        if (!_created)
            return;

        auto *self = const_cast<panel *>(this);
        self->on_native_destroy();
        auto &registry = linux::gemix::panels;
        registry.erase(
            std::remove(registry.begin(), registry.end(), self),
            registry.end());
    }

    void canvas::create() const {
        if (_created)
            return;

        wnd *parent = get_parent();
        if (!parent || !parent->get_created())
            throw std::runtime_error(
                "GEM: canvas requires a created parent.");

        auto *self = const_cast<canvas *>(this);
        linux::gemix::canvases.push_back(self);
        _created = true;
        self->synchronize_theme_metrics();
        self->relayout_children();
        self->on_native_create();
    }

    void canvas::show() const {
        if (!_created)
            throw std::runtime_error("GEM: canvas is not created.");
        invalidate();
    }

    void canvas::destroy() const {
        if (!_created)
            return;

        auto *self = const_cast<canvas *>(this);
        self->on_native_destroy();
        auto &registry = linux::gemix::canvases;
        registry.erase(
            std::remove(registry.begin(), registry.end(), self),
            registry.end());
    }
} // namespace native
