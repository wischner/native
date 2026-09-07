//
// Implements the GEM structural panel and paintable canvas as nested
// regions of the emulated-control tree, together with their painting
// and pointer routing.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <vector>

#include <native.h>
#include <native/canvas.h>
#include <native/panel.h>

#include "../../gpx_wnd.h"
#include "globals.h"
#include "../../table_render.h"
#include "../../collection_render.h"

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

    // Use the same theme metrics and geometry as collection painting.
    native::detail::collection_scrollbar collection_scrollbar_for(
        native::collection_view &control, native::app_wnd &owner) {
        const auto metrics = native::theme::create(owner.get_gpx())->defaults();
        if (auto *icons = dynamic_cast<native::icon_view *>(&control))
            return native::detail::make_collection_scrollbar(*icons, metrics);
        if (auto *tree = dynamic_cast<native::tree_view *>(&control))
            return native::detail::make_collection_scrollbar(*tree, metrics);
        return {};
    }

    // Capture every scrollbar press so its release cannot select an item.
    bool begin_collection_scroll(native::collection_view &control,
                                  native::app_wnd &owner,
                                  linux::gemix::gem_window &state,
                                  native::point position) {
        const auto scrollbar = collection_scrollbar_for(control, owner);
        const auto &geometry = scrollbar.geometry;
        if (scrollbar.total <= scrollbar.page ||
            !geometry.bounds.contains(position))
            return false;
        state.capture = &control;
        state.grab_offset = -1;
        if (geometry.thumb.contains(position)) {
            state.grab_offset = position.y - geometry.thumb.y1();
            return true;
        }

        std::int64_t offset = control.get_scroll_offset();
        if (geometry.decrement.contains(position))
            offset -= scrollbar.step;
        else if (geometry.increment.contains(position))
            offset += scrollbar.step;
        else if (position.y < geometry.thumb.y1())
            offset -= static_cast<std::int64_t>(scrollbar.page);
        else if (position.y >= geometry.thumb.y2())
            offset += static_cast<std::int64_t>(scrollbar.page);
        control.set_scroll_offset(static_cast<int>(std::clamp<std::int64_t>(
            offset, 0, std::numeric_limits<int>::max())));
        return true;
    }

    // Move only captured thumbs; arrow and page presses retain capture.
    void drag_collection_scroll(native::collection_view &control,
                                 native::app_wnd &owner,
                                 int grab_offset,
                                 native::point position) {
        if (grab_offset < 0) return;
        const auto scrollbar = collection_scrollbar_for(control, owner);
        const auto value = native::detail::classic_scrollbar_drag_value(
            scrollbar.geometry, native::scrollbar_orientation::vertical,
            position.y, grab_offset, scrollbar.total, scrollbar.page);
        control.set_scroll_offset(static_cast<int>(std::min<std::uint64_t>(
            value, std::numeric_limits<int>::max())));
    }
} // namespace

namespace linux::gemix
{
    void forget_drag(native::wnd *control) {
        // A borrowed control may have changed parent since capture began.
        for (auto *owner : windows) {
            auto *state = window_states.object_from_handle(owner);
            if (state && state->capture == control) {
                state->capture = nullptr;
                state->grab_offset = -1;
            }
        }
    }

    bool dispatch_drag_click(native::app_wnd *owner, native::point point,
                             bool pressed) {
        auto *state = window_states.object_from_handle(owner);
        if (!state) return false;
        if (!pressed && state->capture) {
            auto *control = state->capture;
            if (dynamic_cast<native::collection_view *>(control))
                dispatch_drag_move(owner, point);
            state->capture = nullptr;
            state->grab_offset = -1;
            if (dynamic_cast<native::split_view *>(control) ||
                dynamic_cast<native::canvas *>(control))
                control->on_native_mouse_click(native::mouse_event(
                    native::mouse_button::left, native::mouse_action::release,
                    local_point(*control, point)));
            return true;
        }
        if (!pressed) return false;
        auto *control = native::detail::deepest_at(*owner, point);
        if (auto *split = dynamic_cast<native::split_view *>(control)) {
            if (!split->get_splitter_bounds().contains(local_point(*split, point)))
                return false;
            state->capture = split;
            split->on_native_mouse_click(native::mouse_event(
                native::mouse_button::left, native::mouse_action::press,
                local_point(*split, point)));
            return true;
        }
        if (auto *surface = dynamic_cast<native::canvas *>(control)) {
            state->capture = surface;
            surface->on_native_mouse_click(native::mouse_event(
                native::mouse_button::left, native::mouse_action::press,
                local_point(*surface, point)));
            return true;
        }
        if (auto *table = dynamic_cast<native::table_view *>(control)) {
            if (native::detail::begin_table_scrollbar_drag(*table,
                local_point(*table, point), state->horizontal, state->grab_offset)) {
                state->capture = table;
                return true;
            }
        }
        if (auto *collection = dynamic_cast<native::collection_view *>(control))
            return begin_collection_scroll(*collection, *owner, *state,
                                           local_point(*collection, point));
        return false;
    }

    bool dispatch_drag_move(native::app_wnd *owner, native::point point) {
        auto *state = window_states.object_from_handle(owner);
        if (!state || !state->capture) return false;
        auto *control = state->capture;
        if (auto *table = dynamic_cast<native::table_view *>(control))
            native::detail::drag_table_scrollbar(*table, local_point(*table, point),
                state->horizontal, state->grab_offset);
        else if (auto *collection = dynamic_cast<native::collection_view *>(control))
            drag_collection_scroll(*collection, *owner, state->grab_offset,
                                   local_point(*collection, point));
        else control->on_native_mouse_move(local_point(*control, point));
        return true;
    }

    void render_surfaces(native::app_wnd *parent, native::gpx &graphics) {
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
        for (auto *control : tab_views) {
            if (created_in(control, parent))
                regions.push_back(control);
        }
        for (auto *control : accordions) {
            if (created_in(control, parent))
                regions.push_back(control);
        }
        // Split hosts have no registry. Collect their live ancestors
        // before sorting every structural surface in one paint pass.
        const auto descendants = regions;
        for (auto *control : descendants) {
            for (auto *ancestor = control->get_parent();
                 ancestor && ancestor != parent;
                 ancestor = ancestor->get_parent()) {
                if (dynamic_cast<native::split_view *>(ancestor) &&
                    created_in(ancestor, parent) &&
                    std::find(regions.begin(), regions.end(), ancestor) ==
                        regions.end())
                    regions.push_back(ancestor);
            }
        }
        std::stable_sort(regions.begin(),
                         regions.end(),
                         [](native::wnd *left, native::wnd *right) {
                             return depth_of(*left) < depth_of(*right);
                         });

        for (native::wnd *region : regions) {
            const native::rect bounds = root_bounds(*region);
            const auto work = work_rect(wnd_bindings.handle_from_object(parent));
            const native::point screen_origin(bounds.p.x + work.p.x,
                                               bounds.p.y + work.p.y);
            if (!bounds.d.w || !bounds.d.h)
                continue;

            if (auto *tabs = dynamic_cast<native::tab_view *>(region)) {
                render_tab_view(tabs, graphics);
                continue;
            }
            if (auto *accordion = dynamic_cast<native::accordion *>(region)) {
                native::detail::draw_accordion_at(
                    *accordion, graphics, origin_in_root(*accordion));
                continue;
            }
            if (auto *split = dynamic_cast<native::split_view *>(region)) {
                native::detail::draw_split_view_at(
                    *split, graphics, origin_in_root(*split));
                continue;
            }
            if (auto *host = dynamic_cast<native::panel *>(region)) {
                native::gpx_wnd region_gpx(parent, screen_origin);
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
            native::gpx_wnd region_gpx(parent, screen_origin);
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
                                bool pressed,
                                native::mouse_button button) {
        if (auto *surface =
                region_at<native::canvas>(parent, position)) {
            surface->on_native_mouse_click(native::mouse_event(
                button,
                pressed ? native::mouse_action::press
                        : native::mouse_action::release,
                local_point(*surface, position)));
            return true;
        }
        // Nothing else claimed the position, so this is empty panel
        // space. The panel reports it and adds no action of its own.
        if (auto *host = region_at<native::panel>(parent, position)) {
            host->on_native_mouse_click(native::mouse_event(
                button,
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
        linux::gemix::forget_drag(this);
        if (!_created)
            return;

        auto *self = this;
        auto &registry = linux::gemix::canvases;
        registry.erase(
            std::remove(registry.begin(), registry.end(), self),
            registry.end());
    }
} // namespace native
