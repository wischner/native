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

    bool hit(native::wnd &control, int x, int y) {
        return root_bounds(control).contains(
            native::point(static_cast<native::coord>(x),
                          static_cast<native::coord>(y)));
    }

    //
    // Return the visible canvas under a root-space point.
    //
    // Notes:
    //      Canvases are leaves, so the deepest match is the one the
    //      pointer is actually over when regions overlap.
    //
    native::canvas *canvas_at(native::wnd *owner, int x, int y) {
        native::canvas *found = nullptr;
        int best = -1;
        for (auto *control : linux::sdl2::canvases) {
            if (!control || !visible(*control) ||
                root_of(control) != owner || !hit(*control, x, y))
                continue;
            const int depth = depth_of(*control);
            if (depth > best) {
                best = depth;
                found = control;
            }
        }
        return found;
    }

    // Return the deepest visible panel under a root-space point.
    native::panel *panel_at(native::wnd *owner, int x, int y) {
        native::panel *found = nullptr;
        int best = -1;
        for (auto *control : linux::sdl2::panels) {
            if (!control || !visible(*control) ||
                root_of(control) != owner || !hit(*control, x, y))
                continue;
            const int depth = depth_of(*control);
            if (depth > best) {
                best = depth;
                found = control;
            }
        }
        return found;
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

        auto appearance = native::theme::create(graphics);
        for (native::wnd *region : regions) {
            const native::rect bounds = root_bounds(*region);
            if (!bounds.d.w || !bounds.d.h)
                continue;

            if (auto *host = dynamic_cast<native::panel *>(region)) {
                (void)host;
                // A panel is visually empty. Repainting the themed
                // container surface is what stops stale pixels from
                // showing through where no child covers it.
                auto saved = graphics.save_state();
                graphics.set_clip(bounds);
                appearance->draw_surface(bounds,
                                         native::surface_kind::panel,
                                         native::theme::state{});
                continue;
            }

            auto *surface = dynamic_cast<native::canvas *>(region);
            if (!surface || !renderer)
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

            const native::rect invalid(0,
                                       0,
                                       surface->get_dimensions().w,
                                       surface->get_dimensions().h);
            graphics.set_clip(invalid);
            native::wnd_paint_event event(invalid, graphics);
            surface->on_native_paint(event);

            SDL_RenderSetViewport(renderer, &content_viewport);
            graphics.set_clip(content_bounds);
        }
    }

    bool handle_canvas_mouse(
        native::wnd *owner, int x, int y, bool pressed, bool released) {
        native::canvas *surface = canvas_at(owner, x, y);
        if (!surface || (!pressed && !released))
            return false;

        surface->on_native_mouse_click(native::mouse_event(
            native::mouse_button::left,
            pressed ? native::mouse_action::press
                    : native::mouse_action::release,
            local_point(*surface, x, y)));
        return true;
    }

    bool handle_canvas_motion(native::wnd *owner, int x, int y) {
        native::canvas *surface = canvas_at(owner, x, y);
        if (!surface)
            return false;
        surface->on_native_mouse_move(local_point(*surface, x, y));
        return true;
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
    void panel::create() const {
        if (_created)
            return;

        wnd *parent = get_parent();
        if (!parent)
            throw std::runtime_error(
                "SDL2: panel requires a parent window.");
        if (!parent->get_created())
            throw std::runtime_error(
                "SDL2: panel parent is not created.");

        auto *self = const_cast<panel *>(this);
        linux::sdl2::panel_bindings.register_pair(
            self, new linux::sdl2::sdl2_surface());
        linux::sdl2::panels.push_back(self);
        _created = true;
        self->on_native_create();
    }

    void panel::show() const {
        auto *state = linux::sdl2::panel_bindings.object_from_handle(
            const_cast<panel *>(this));
        if (!_created || !state)
            throw std::runtime_error("SDL2: panel is not created.");
        state->visible = true;
        invalidate();
    }

    void panel::destroy() const {
        if (!_created)
            return;

        auto *self = const_cast<panel *>(this);
        auto *state = linux::sdl2::panel_bindings.object_from_handle(self);
        self->on_native_destroy();
        auto &registry = linux::sdl2::panels;
        registry.erase(
            std::remove(registry.begin(), registry.end(), self),
            registry.end());
        if (state) {
            linux::sdl2::panel_bindings.unregister_by_handle(self);
            delete state;
        }
    }

    void canvas::create() const {
        if (_created)
            return;

        wnd *parent = get_parent();
        if (!parent)
            throw std::runtime_error(
                "SDL2: canvas requires a parent window.");
        if (!parent->get_created())
            throw std::runtime_error(
                "SDL2: canvas parent is not created.");

        auto *self = const_cast<canvas *>(this);
        linux::sdl2::canvas_bindings.register_pair(
            self, new linux::sdl2::sdl2_surface());
        linux::sdl2::canvases.push_back(self);
        _created = true;
        self->synchronize_theme_metrics();
        self->relayout_children();
        self->on_native_create();
    }

    void canvas::show() const {
        auto *state = linux::sdl2::canvas_bindings.object_from_handle(
            const_cast<canvas *>(this));
        if (!_created || !state)
            throw std::runtime_error("SDL2: canvas is not created.");
        state->visible = true;
        invalidate();
    }

    void canvas::destroy() const {
        if (!_created)
            return;

        auto *self = const_cast<canvas *>(this);
        auto *state =
            linux::sdl2::canvas_bindings.object_from_handle(self);
        self->on_native_destroy();
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
