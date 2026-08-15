//
// Implements the SDL2 window backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <SDL2/SDL.h>

#include <native.h>
#include "bindings.h"
#include "gpx_wnd.h"
#include "globals.h"

namespace native
{
    void wnd::apply_position() {
        if (auto *control = dynamic_cast<button *>(this)) {
            auto *binding =
                linux::sdl2::button_bindings.object_from_handle(control);
            if (binding) {
                binding->bounds = _bounds;
                if (binding->parent)
                    binding->parent->invalidate();
            }
            return;
        }

        SDL_Window *window =
            linux::sdl2::wnd_bindings.handle_from_object(this);
        if (window) {
            SDL_SetWindowPosition(
                window, _bounds.p.x, _bounds.p.y);
        }
    }

    void wnd::apply_dimensions() {
        if (auto *control = dynamic_cast<button *>(this)) {
            auto *binding =
                linux::sdl2::button_bindings.object_from_handle(control);
            if (binding) {
                binding->bounds = _bounds;
                if (binding->parent)
                    binding->parent->invalidate();
            }
            return;
        }

        SDL_Window *window =
            linux::sdl2::wnd_bindings.handle_from_object(this);
        if (window) {
            SDL_SetWindowSize(
                window, _bounds.d.w, _bounds.d.h);
        }
    }

    void wnd::apply_bounds() {
        if (auto *control = dynamic_cast<button *>(this)) {
            auto *binding =
                linux::sdl2::button_bindings.object_from_handle(control);
            if (binding) {
                binding->bounds = _bounds;
                if (binding->parent)
                    binding->parent->invalidate();
            }
            return;
        }

        SDL_Window *window =
            linux::sdl2::wnd_bindings.handle_from_object(this);
        if (window) {
            SDL_SetWindowPosition(
                window, _bounds.p.x, _bounds.p.y);
            SDL_SetWindowSize(
                window, _bounds.d.w, _bounds.d.h);
        }
    }

    void wnd::apply_parent() {
        auto *control = dynamic_cast<button *>(this);
        if (!control)
            return;

        auto *binding =
            linux::sdl2::button_bindings.object_from_handle(control);
        if (!binding)
            return;

        if (binding->parent)
            binding->parent->invalidate();
        binding->parent = _parent;
        if (binding->parent)
            binding->parent->invalidate();
    }

    wnd &wnd::invalidate() const {
        if (!_created)
            return const_cast<wnd &>(*this);

        if (auto *control = dynamic_cast<const button *>(this)) {
            auto *binding = linux::sdl2::button_bindings.object_from_handle(
                const_cast<button *>(control));
            if (binding && binding->parent)
                binding->parent->invalidate();
            return const_cast<wnd &>(*this);
        }

        if (auto *cache = linux::sdl2::wnd_gpx_bindings.object_from_handle(
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
            throw std::runtime_error("Cannot obtain gpx before window is created.");

        if (!_gpx)
            _gpx = new gpx_wnd(this);

        return *_gpx;
    }

} // namespace native
