//
// Implements the SDL2 structural panel and paintable canvas as nested
// regions of the emulated-control tree, together with their painting,
// clipping, and pointer routing.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>
#include <vector>

#include <SDL2/SDL.h>

#include <native.h>
#include <native/canvas.h>
#include <native/panel.h>

#include "../../collection_render.h"
#include "globals.h"

namespace
{
    using linux::sdl2::depth_of;
    using linux::sdl2::origin_in_root;
    using linux::sdl2::root_bounds;
    using linux::sdl2::root_of;

    bool visible(native::panel &control) {
        auto *state =
            linux::sdl2::panel_bindings.object_from_handle(&control);
        return state && state->visible && control.get_created();
    }

    bool visible(native::canvas &control) {
        auto *state =
            linux::sdl2::canvas_bindings.object_from_handle(&control);
        return state && state->visible && control.get_created();
    }

    //
    // Return the visible canvas under a root-space point.
    //
    // Notes:
    //      Canvases are leaves, so the deepest match is the one the
    //      pointer is actually over when regions overlap.
    //
    native::canvas *canvas_at(native::wnd *owner, int x, int y) {
        if (!owner)
            return nullptr;
        return dynamic_cast<native::canvas *>(
            native::detail::deepest_at(
                *owner,
                native::point(static_cast<native::coord>(x),
                              static_cast<native::coord>(y))));
    }

    // Recover capture from live peer state; destruction leaves no
    // process-wide raw pointer waiting for the next mouse event.
    native::canvas *captured_canvas(native::wnd *owner) {
        for (auto *surface : linux::sdl2::canvases) {
            if (!surface || root_of(surface) != owner ||
                !visible(*surface))
                continue;
            auto *state = linux::sdl2::canvas_bindings
                              .object_from_handle(surface);
            if (state && state->pressed_buttons != 0)
                return surface;
        }
        return nullptr;
    }

    // Return the deepest visible panel under a root-space point.
    native::panel *panel_at(native::wnd *owner, int x, int y) {
        if (!owner)
            return nullptr;
        return dynamic_cast<native::panel *>(
            native::detail::deepest_at(
                *owner,
                native::point(static_cast<native::coord>(x),
                              static_cast<native::coord>(y))));
    }

    // Convert a root-space position into a control-local one.
    native::point local_point(native::wnd &control, int x, int y) {
        const native::point origin = origin_in_root(control);
        return native::point(static_cast<native::coord>(x - origin.x),
                             static_cast<native::coord>(y - origin.y));
    }
} // namespace

namespace linux::sdl2
{
    void render_surfaces(native::wnd *owner, native::gpx &graphics) {
        // Regions are painted parent first so a container never erases
        // the descendants drawn inside it.
        std::vector<native::wnd *> regions;
        for (auto *control : panels) {
            if (control && visible(*control) && root_of(control) == owner)
                regions.push_back(control);
        }
        for (auto *control : canvases) {
            if (control && visible(*control) && root_of(control) == owner)
                regions.push_back(control);
        }
        // All structural hosts share this depth-ordered pass. Painting
        // tabs or accordion backgrounds in a later type-specific pass
        // would cover their already-painted document canvases.
        const auto add_host = [&regions, owner](native::wnd *control) {
            if (control && control->get_created() &&
                control->get_visible() && root_of(control) == owner)
                regions.push_back(control);
        };
        for (auto *control : tab_views)
            add_host(control);
        for (auto *control : split_views)
            add_host(control);
        for (auto *control : accordions)
            add_host(control);
        std::stable_sort(regions.begin(),
                         regions.end(),
                         [](native::wnd *left, native::wnd *right) {
                             return depth_of(*left) < depth_of(*right);
                         });

        // Restoring the window content viewport after each canvas
        // keeps later emulated controls in root coordinates.
        auto *cache = wnd_gpx_bindings.object_from_handle(owner);
        SDL_Renderer *renderer = cache ? cache->renderer : nullptr;
        const int content_origin = content_origin_y(owner);
        const native::rect content_bounds(
            0, 0, owner->get_dimensions().w, owner->get_dimensions().h);
        const SDL_Rect content_viewport = {
            0,
            content_origin,
            static_cast<int>(content_bounds.d.w),
            static_cast<int>(content_bounds.d.h)};

        for (native::wnd *region : regions) {
            const native::rect bounds = root_bounds(*region);
            if (!bounds.d.w || !bounds.d.h)
                continue;

            if (!renderer)
                continue;

            //
            // The canvas paints its own client, rulers, and
            // scrollbars in canvas-local coordinates. An SDL viewport
            // supplies that origin and clips to the region in one
            // step, so application drawing cannot reach a sibling,
            // the parent, or its own scrollbar tracks.
            //
            // Captured before the viewport changes, so the restored
            // clip is expressed in the window's own space again.
            auto saved = graphics.save_state();

            const SDL_Rect region_viewport = {
                bounds.p.x,
                bounds.p.y + content_origin,
                static_cast<int>(bounds.d.w),
                static_cast<int>(bounds.d.h)};
            SDL_RenderSetViewport(renderer, &region_viewport);

            const native::rect invalid(0, 0, bounds.d.w, bounds.d.h);
            graphics.set_clip(invalid);
            region->on_native_paint(
                native::wnd_paint_event(invalid, graphics));

            SDL_RenderSetViewport(renderer, &content_viewport);
            graphics.set_clip(content_bounds);
        }
    }

    bool handle_canvas_mouse(
        native::wnd *owner, int x, int y, bool pressed, bool released,
        native::mouse_button button, bool captured_only) {
        if ((!pressed && !released) ||
            button == native::mouse_button::none)
            return false;
        native::canvas *surface = captured_canvas(owner);
        if (!surface && !captured_only)
            surface = canvas_at(owner, x, y);
        if (!surface)
            return false;

        auto *state = canvas_bindings.object_from_handle(surface);
        const auto mask = std::uint32_t{1} <<
            static_cast<unsigned int>(button);
        // Update capture before callbacks, which may destroy the page.
        if (pressed)
            state->pressed_buttons |= mask;
        else
            state->pressed_buttons &= ~mask;
        surface->on_native_mouse_click(native::mouse_event(
            button,
            pressed ? native::mouse_action::press
                    : native::mouse_action::release,
            local_point(*surface, x, y)));
        return true;
    }

    bool handle_canvas_motion(native::wnd *owner, int x, int y,
                              bool captured_only) {
        native::canvas *surface = captured_canvas(owner);
        if (!surface && !captured_only)
            surface = canvas_at(owner, x, y);
        if (!surface)
            return false;
        surface->on_native_mouse_move(local_point(*surface, x, y));
        return true;
    }

    void release_canvas_capture(native::wnd *owner) {
        while (native::canvas *surface = captured_canvas(owner)) {
            auto *state = canvas_bindings.object_from_handle(surface);
            unsigned int button = 1;
            while ((state->pressed_buttons &
                    (std::uint32_t{1} << button)) == 0)
                ++button;
            state->pressed_buttons &= ~(std::uint32_t{1} << button);
            surface->on_native_mouse_click(native::mouse_event(
                static_cast<native::mouse_button>(button),
                native::mouse_action::release, native::point(-1, -1)));
        }
    }

    bool handle_canvas_wheel(native::wnd *owner,
                             int x,
                             int y,
                             int delta) {
        native::canvas *surface = canvas_at(owner, x, y);
        if (!surface)
            return false;
        surface->on_native_mouse_wheel(native::mouse_wheel_event(
            local_point(*surface, x, y),
            static_cast<native::coord>(delta),
            native::wheel_direction::vertical));
        return true;
    }

    bool handle_panel_mouse(
        native::wnd *owner, int x, int y, bool pressed, bool released) {
        native::panel *host = panel_at(owner, x, y);
        if (!host || (!pressed && !released))
            return false;

        // Nothing else claimed the position, so this is empty panel
        // space. The panel reports it and adds no action of its own.
        host->on_native_mouse_click(native::mouse_event(
            native::mouse_button::left,
            pressed ? native::mouse_action::press
                    : native::mouse_action::release,
            local_point(*host, x, y)));
        return true;
    }

    bool handle_panel_motion(native::wnd *owner, int x, int y) {
        native::panel *host = panel_at(owner, x, y);
        if (!host)
            return false;
        host->on_native_mouse_move(local_point(*host, x, y));
        return true;
    }

    bool handle_panel_wheel(native::wnd *owner, int x, int y, int delta) {
        native::panel *host = panel_at(owner, x, y);
        if (!host)
            return false;
        host->on_native_mouse_wheel(native::mouse_wheel_event(
            local_point(*host, x, y),
            static_cast<native::coord>(delta),
            native::wheel_direction::vertical));
        return true;
    }
} // namespace linux::sdl2

namespace native
{
    void panel::create_native() {
        wnd *parent = get_parent();
        if (!parent)
            throw std::runtime_error(
                "SDL2: panel requires a parent window.");
        if (!parent->get_created())
            throw std::runtime_error(
                "SDL2: panel parent is not created.");

        auto *self = this;
        linux::sdl2::panel_bindings.register_pair(
            self, new linux::sdl2::sdl2_surface());
        linux::sdl2::panels.push_back(self);
    }

    void panel::show_native() {
        auto *state = linux::sdl2::panel_bindings.object_from_handle(
            this);
        if (!_created || !state)
            throw std::runtime_error("SDL2: panel is not created.");
        state->visible = true;
        invalidate();
    }

    void panel::destroy_native() {
        if (!_created)
            return;

        auto *self = this;
        auto *state = linux::sdl2::panel_bindings.object_from_handle(self);
        auto &registry = linux::sdl2::panels;
        registry.erase(
            std::remove(registry.begin(), registry.end(), self),
            registry.end());
        if (state) {
            linux::sdl2::panel_bindings.unregister_by_handle(self);
            delete state;
        }
    }

    void canvas::create_native() {
        wnd *parent = get_parent();
        if (!parent)
            throw std::runtime_error(
                "SDL2: canvas requires a parent window.");
        if (!parent->get_created())
            throw std::runtime_error(
                "SDL2: canvas parent is not created.");

        auto *self = this;
        linux::sdl2::canvas_bindings.register_pair(
            self, new linux::sdl2::sdl2_surface());
        linux::sdl2::canvases.push_back(self);
        self->synchronize_theme_metrics();
        self->relayout_children();
    }

    void canvas::show_native() {
        auto *state = linux::sdl2::canvas_bindings.object_from_handle(
            this);
        if (!_created || !state)
            throw std::runtime_error("SDL2: canvas is not created.");
        state->visible = true;
        invalidate();
    }

    void canvas::destroy_native() {
        if (!_created)
            return;

        auto *self = this;
        auto *state =
            linux::sdl2::canvas_bindings.object_from_handle(self);
        auto &registry = linux::sdl2::canvases;
        registry.erase(
            std::remove(registry.begin(), registry.end(), self),
            registry.end());
        if (state) {
            linux::sdl2::canvas_bindings.unregister_by_handle(self);
            delete state;
        }
    }
} // namespace native
