//
// Implements the SDL2 window backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <limits>
#include <stdexcept>

#include <SDL2/SDL.h>

#include <native.h>
#include <native/wnd.h>
#include "bindings.h"
#include "gpx_wnd.h"
#include "globals.h"
#include "window_position.h"

namespace
{
    template <typename public_type, typename registry_type>
    bool update_bounds(native::wnd *window,
                       const native::rect &bounds,
                       registry_type &registry) {
        auto *control = dynamic_cast<public_type *>(window);
        if (!control)
            return false;
        (void)bounds;
        auto *binding = registry.object_from_handle(control);
        if (binding) {
            // Emulated controls are painted and hit-tested in their
            // root window's space, so a control nested inside panels
            // caches the accumulated origin rather than the bounds
            // its own parent gave it.
            binding->bounds = linux::sdl2::root_bounds(*control);
            if (binding->parent)
                binding->parent->invalidate();
        }
        return true;
    }

    template <typename public_type, typename registry_type>
    bool update_parent(native::wnd *window,
                       native::wnd *parent,
                       registry_type &registry) {
        auto *control = dynamic_cast<public_type *>(window);
        if (!control)
            return false;
        auto *binding = registry.object_from_handle(control);
        if (binding) {
            if (binding->parent)
                binding->parent->invalidate();
            binding->parent = parent;
            if (binding->parent)
                binding->parent->invalidate();
        }
        return true;
    }

    template <typename public_type, typename registry_type>
    native::wnd *control_parent(native::wnd *window,
                                registry_type &registry) {
        auto *control = dynamic_cast<public_type *>(window);
        if (!control)
            return nullptr;
        auto *binding = registry.object_from_handle(control);
        return binding ? binding->parent : nullptr;
    }

    bool update_control_bounds(native::wnd *window,
                               const native::rect &bounds) {
        if (dynamic_cast<native::panel *>(window) ||
            dynamic_cast<native::canvas *>(window) ||
            dynamic_cast<native::accordion *>(window) ||
            dynamic_cast<native::tab_view *>(window) ||
            dynamic_cast<native::icon_view *>(window) ||
            dynamic_cast<native::tree_view *>(window) ||
            dynamic_cast<native::table_view *>(window)) {
            if (window->get_parent())
                window->get_parent()->invalidate(bounds);
            return true;
        }
        return update_bounds<native::button>(
                   window, bounds, linux::sdl2::button_bindings) ||
               update_bounds<native::check>(
                   window, bounds, linux::sdl2::check_bindings) ||
               update_bounds<native::radio>(
                   window, bounds, linux::sdl2::radio_bindings) ||
               update_bounds<native::list>(
                   window, bounds, linux::sdl2::list_bindings) ||
               update_bounds<native::text_edit>(
                   window, bounds, linux::sdl2::text_edit_bindings);
    }

    bool update_control_parent(native::wnd *window,
                               native::wnd *parent) {
        if (dynamic_cast<native::panel *>(window) ||
            dynamic_cast<native::canvas *>(window) ||
            dynamic_cast<native::accordion *>(window) ||
            dynamic_cast<native::tab_view *>(window) ||
            dynamic_cast<native::icon_view *>(window) ||
            dynamic_cast<native::tree_view *>(window) ||
            dynamic_cast<native::table_view *>(window)) {
            if (parent)
                parent->invalidate();
            return true;
        }
        return update_parent<native::button>(
                   window, parent, linux::sdl2::button_bindings) ||
               update_parent<native::check>(
                   window, parent, linux::sdl2::check_bindings) ||
               update_parent<native::radio>(
                   window, parent, linux::sdl2::radio_bindings) ||
               update_parent<native::list>(
                   window, parent, linux::sdl2::list_bindings) ||
               update_parent<native::text_edit>(
                   window, parent, linux::sdl2::text_edit_bindings);
    }

    native::wnd *emulated_parent(native::wnd *window) {
        if (dynamic_cast<native::panel *>(window) ||
            dynamic_cast<native::canvas *>(window) ||
            dynamic_cast<native::accordion *>(window) ||
            dynamic_cast<native::tab_view *>(window) ||
            dynamic_cast<native::icon_view *>(window) ||
            dynamic_cast<native::tree_view *>(window) ||
            dynamic_cast<native::table_view *>(window)) {
            return window->get_parent();
        }
        if (auto *parent = control_parent<native::button>(
                window, linux::sdl2::button_bindings))
            return parent;
        if (auto *parent = control_parent<native::check>(
                window, linux::sdl2::check_bindings))
            return parent;
        if (auto *parent = control_parent<native::radio>(
                window, linux::sdl2::radio_bindings))
            return parent;
        if (auto *parent = control_parent<native::list>(
                window, linux::sdl2::list_bindings))
            return parent;
        return control_parent<native::text_edit>(
            window, linux::sdl2::text_edit_bindings);
    }
} // namespace

namespace
{
    // Convert a cached client size to the window size that holds it.
    // Public geometry is the client area, but SDL2 sizes the whole
    // window, so a window carrying a menu bar has to be that much
    // taller to leave the requested client behind it. Reporting a
    // resize converts the same way in reverse.
    native::size window_size_for(native::wnd *window,
                                 const native::size &client) {
        const int limit =
            std::numeric_limits<native::dim>::max();
        const int height =
            static_cast<int>(client.h) +
            linux::sdl2::content_origin_y(window);

        return native::size(client.w,
                            static_cast<native::dim>(
                                std::min(height, limit)));
    }

    // Apply a window's cached size and keep its title bar reachable.
    void apply_window_geometry(native::wnd *owner,
                               SDL_Window *window,
                               const native::rect &bounds,
                               bool resize) {
        const native::size outer = window_size_for(owner, bounds.d);
        if (resize)
            SDL_SetWindowSize(window, outer.w, outer.h);

        const native::point position =
            linux::sdl2::constrain_window_position(
                window, bounds.p, outer);
        SDL_SetWindowPosition(window, position.x, position.y);
    }
} // namespace

namespace native
{
    void wnd::apply_position() {
        if (update_control_bounds(this, _bounds))
            return;

        SDL_Window *window =
            linux::sdl2::wnd_bindings.handle_from_object(this);
        if (window)
            apply_window_geometry(this, window, _bounds, false);
    }

    void wnd::apply_dimensions() {
        if (update_control_bounds(this, _bounds))
            return;

        SDL_Window *window =
            linux::sdl2::wnd_bindings.handle_from_object(this);
        if (window)
            apply_window_geometry(this, window, _bounds, true);
    }

    void wnd::apply_bounds() {
        if (update_control_bounds(this, _bounds))
            return;

        SDL_Window *window =
            linux::sdl2::wnd_bindings.handle_from_object(this);
        if (window)
            apply_window_geometry(this, window, _bounds, true);
    }

    void wnd::apply_parent() {
        update_control_parent(this, _parent);
    }

    wnd &wnd::invalidate() const {
        if (!_created)
            return const_cast<wnd &>(*this);

        if (auto *parent = emulated_parent(const_cast<wnd *>(this))) {
            parent->invalidate();
            return const_cast<wnd &>(*this);
        }

        if (auto *cache =
                linux::sdl2::wnd_gpx_bindings.object_from_handle(
                    const_cast<wnd *>(this)))
            cache->invalidated = true;

        return const_cast<wnd &>(*this);
    }

    wnd &wnd::invalidate(const rect &) const {
        // SDL2 does not expose partial window invalidation.
        return invalidate();
    }

    gpx &wnd::get_gpx() const {
        if (!_created)
            throw std::runtime_error(
                "Cannot obtain gpx before window is created.");

        if (!_gpx)
            _gpx = new gpx_wnd(this);

        return *_gpx;
    }

} // namespace native
