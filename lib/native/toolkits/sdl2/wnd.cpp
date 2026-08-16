//
// Implements the SDL2 window backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <SDL2/SDL.h>

#include <native.h>
#include <native/wnd.h>
#include "bindings.h"
#include "gpx_wnd.h"
#include "globals.h"

namespace
{
    template <typename public_type, typename registry_type>
    bool update_bounds(native::wnd *window,
                       const native::rect &bounds,
                       registry_type &registry) {
        auto *control = dynamic_cast<public_type *>(window);
        if (!control)
            return false;
        auto *binding = registry.object_from_handle(control);
        if (binding) {
            binding->bounds = bounds;
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
        return update_bounds<native::button>(
                   window, bounds, linux::sdl2::button_bindings) ||
               update_bounds<native::check>(
                   window, bounds, linux::sdl2::check_bindings) ||
               update_bounds<native::radio>(
                   window, bounds, linux::sdl2::radio_bindings) ||
               update_bounds<native::list>(
                   window, bounds, linux::sdl2::list_bindings);
    }

    bool update_control_parent(native::wnd *window,
                               native::wnd *parent) {
        return update_parent<native::button>(
                   window, parent, linux::sdl2::button_bindings) ||
               update_parent<native::check>(
                   window, parent, linux::sdl2::check_bindings) ||
               update_parent<native::radio>(
                   window, parent, linux::sdl2::radio_bindings) ||
               update_parent<native::list>(
                   window, parent, linux::sdl2::list_bindings);
    }

    native::wnd *emulated_parent(native::wnd *window) {
        if (auto *parent = control_parent<native::button>(
                window, linux::sdl2::button_bindings))
            return parent;
        if (auto *parent = control_parent<native::check>(
                window, linux::sdl2::check_bindings))
            return parent;
        if (auto *parent = control_parent<native::radio>(
                window, linux::sdl2::radio_bindings))
            return parent;
        return control_parent<native::list>(window,
                                            linux::sdl2::list_bindings);
    }
} // namespace

namespace native
{
    void wnd::apply_position() {
        if (update_control_bounds(this, _bounds))
            return;

        SDL_Window *window =
            linux::sdl2::wnd_bindings.handle_from_object(this);
        if (window) {
            SDL_SetWindowPosition(window, _bounds.p.x, _bounds.p.y);
        }
    }

    void wnd::apply_dimensions() {
        if (update_control_bounds(this, _bounds))
            return;

        SDL_Window *window =
            linux::sdl2::wnd_bindings.handle_from_object(this);
        if (window) {
            SDL_SetWindowSize(window, _bounds.d.w, _bounds.d.h);
        }
    }

    void wnd::apply_bounds() {
        if (update_control_bounds(this, _bounds))
            return;

        SDL_Window *window =
            linux::sdl2::wnd_bindings.handle_from_object(this);
        if (window) {
            SDL_SetWindowPosition(window, _bounds.p.x, _bounds.p.y);
            SDL_SetWindowSize(window, _bounds.d.w, _bounds.d.h);
        }
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
