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
    template <typename state_type>
    bool update_bounds(native::wnd *window,
                       const native::rect &) {
        auto *binding = native::detail::peer_state<state_type>(*window);
        if (binding) {
            // Emulated controls are painted and hit-tested in their
            // root window's space, so a control nested inside panels
            // caches the accumulated origin rather than the bounds
            // its own parent gave it.
            binding->bounds = linux::sdl2::root_bounds(*window);
            if (binding->parent)
                binding->parent->invalidate();
        }
        return binding != nullptr;
    }

    template <typename state_type>
    bool update_parent(native::wnd *window,
                       native::wnd *parent) {
        auto *binding = native::detail::peer_state<state_type>(*window);
        if (binding) {
            if (binding->parent)
                binding->parent->invalidate();
            binding->parent = parent;
            if (binding->parent)
                binding->parent->invalidate();
        }
        return binding != nullptr;
    }

    bool update_control_bounds(native::wnd *window,
                               const native::rect &bounds) {
        if (!window->get_parent())
            return false;

        const bool updated =
            update_bounds<linux::sdl2::sdl2_button>(window, bounds) ||
            update_bounds<linux::sdl2::sdl2_check>(window, bounds) ||
            update_bounds<linux::sdl2::sdl2_radio>(window, bounds) ||
            update_bounds<linux::sdl2::sdl2_list>(window, bounds);
        if (updated)
            return true;

        if (update_bounds<linux::sdl2::sdl2_text_edit>(window, bounds)) {
            auto *binding = native::detail::peer_state<
                linux::sdl2::sdl2_text_edit>(*window);
            auto *editor = dynamic_cast<native::text_edit *>(window);
            if (binding && editor) {
                const int viewport_width = std::max(
                    1, static_cast<int>(binding->bounds.d.w) - 10);
                const int maximum_scroll = std::max(
                    0,
                    linux::sdl2::text_width(editor->get_text()) -
                        viewport_width);
                binding->horizontal_scroll = std::clamp(
                    binding->horizontal_scroll, 0, maximum_scroll);
            }
            return true;
        }
        window->get_parent()->invalidate(bounds);
        return true;
    }

    bool update_control_parent(native::wnd *window,
                               native::wnd *parent) {
        if (!parent)
            return false;

        const bool updated =
            update_parent<linux::sdl2::sdl2_button>(window, parent) ||
            update_parent<linux::sdl2::sdl2_check>(window, parent) ||
            update_parent<linux::sdl2::sdl2_radio>(window, parent) ||
            update_parent<linux::sdl2::sdl2_list>(window, parent) ||
            update_parent<linux::sdl2::sdl2_combo_box>(window, parent) ||
            update_parent<linux::sdl2::sdl2_text_edit>(window, parent);
        if (!updated)
            parent->invalidate();
        return true;
    }

    native::wnd *emulated_parent(native::wnd *window) {
        return window->get_parent();
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

    void wnd::apply_cursor() {
        wnd *root = linux::sdl2::root_of(this);
        SDL_Window *window = root
                                 ? linux::sdl2::wnd_bindings
                                       .handle_from_object(root)
                                 : nullptr;
        if (!window || SDL_GetMouseFocus() != window)
            return;

        int x = 0;
        int y = 0;
        SDL_GetMouseState(&x, &y);
        y -= linux::sdl2::content_origin_y(root);
        linux::sdl2::update_mouse_cursor(root, point(x, y));
    }

    wnd &wnd::invalidate_native() {
        if (!_created)
            return *this;

        if (auto *parent = emulated_parent(this)) {
            parent->invalidate();
            return *this;
        }

        if (auto *cache =
                linux::sdl2::wnd_gpx_bindings.object_from_handle(
                    this))
            cache->invalidated = true;

        return *this;
    }

    wnd &wnd::invalidate_native(const rect &) {
        // SDL2 does not expose partial window invalidation.
        return invalidate_native();
    }

    gpx &wnd::get_gpx() {
        if (!_created)
            throw std::runtime_error(
                "Cannot obtain gpx before window is created.");

        if (!_gpx)
            _gpx = new gpx_wnd(this);

        return *_gpx;
    }

} // namespace native
