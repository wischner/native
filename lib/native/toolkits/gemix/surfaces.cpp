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
               control->get_visible() &&
               linux::gemix::root_of(control) == owner;
    }

    // Return the deepest visible region of the requested type.
    template <typename control_type>
    control_type *region_at(native::app_wnd *owner,
                            native::point position) {
        if (!owner)
            return nullptr;
        return dynamic_cast<control_type *>(
            native::detail::deepest_at(*owner, position));
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
    void render_surfaces(native::app_wnd *parent, native::gpx &) {
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

        for (native::wnd *region : regions) {
            const native::rect bounds = root_bounds(*region);
            if (!bounds.d.w || !bounds.d.h)
                continue;

            if (auto *host = dynamic_cast<native::panel *>(region)) {
                native::gpx_wnd region_gpx(parent, bounds.p);
                const native::rect invalid(
                    0, 0, bounds.d.w, bounds.d.h);
                region_gpx.set_clip(invalid);
                host->on_native_paint(
                    native::wnd_paint_event(invalid, region_gpx));
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
        if (auto *surface =
                region_at<native::canvas>(parent, position)) {
            surface->on_native_mouse_click(native::mouse_event(
                native::mouse_button::left,
                pressed ? native::mouse_action::press
                        : native::mouse_action::release,
                local_point(*surface, position)));
            return true;
        }
        // Nothing else claimed the position, so this is empty panel
        // space. The panel reports it and adds no action of its own.
        if (auto *host = region_at<native::panel>(parent, position)) {
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
        if (auto *surface =
                region_at<native::canvas>(parent, position)) {
            surface->on_native_mouse_move(
                local_point(*surface, position));
            return true;
        }
        if (auto *host = region_at<native::panel>(parent, position)) {
            host->on_native_mouse_move(local_point(*host, position));
            return true;
        }
        return false;
    }
} // namespace linux::gemix

namespace native
{
    void panel::create_native() {
        wnd *parent = get_parent();
        if (!parent || !parent->get_created())
            throw std::runtime_error(
                "GEM: panel requires a created parent.");

        auto *self = this;
        linux::gemix::panels.push_back(self);
    }

    void panel::show_native() {
        if (!_created)
            throw std::runtime_error("GEM: panel is not created.");
        invalidate();
    }

    void panel::destroy_native() {
        if (!_created)
            return;

        auto *self = this;
        auto &registry = linux::gemix::panels;
        registry.erase(
            std::remove(registry.begin(), registry.end(), self),
            registry.end());
    }

    void canvas::create_native() {
        wnd *parent = get_parent();
        if (!parent || !parent->get_created())
            throw std::runtime_error(
                "GEM: canvas requires a created parent.");

        auto *self = this;
        linux::gemix::canvases.push_back(self);
        self->synchronize_theme_metrics();
        self->relayout_children();
    }

    void canvas::show_native() {
        if (!_created)
            throw std::runtime_error("GEM: canvas is not created.");
        invalidate();
    }

    void canvas::destroy_native() {
        if (!_created)
            return;

        auto *self = this;
        auto &registry = linux::gemix::canvases;
        registry.erase(
            std::remove(registry.begin(), registry.end(), self),
            registry.end());
    }
} // namespace native
