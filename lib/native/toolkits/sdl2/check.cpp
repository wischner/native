//
// Implements the SDL-emulated check control.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <algorithm>
#include <stdexcept>
#include <native.h>
#include <native/check.h>
#include "../../control_render_access.h"
#include "globals.h"
namespace
{
    bool hit(const native::rect &r, int x, int y) {
        return r.contains(native::point(x, y));
    }
} // namespace
namespace linux::sdl2
{
    bool handle_check_motion(native::wnd *owner, int x, int y) {
        bool changed = false;
        for (auto *c : checks) {
            auto *b = check_bindings.object_from_handle(c);
            if (!b || root_of(c) != owner || !b->visible)
                continue;
            bool h = hit(root_bounds(*c), x, y);
            if (h != b->hover) {
                b->hover = h;
                changed = true;
            }
        }
        if (changed)
            owner->invalidate();
        return changed;
    }
    bool handle_check_mouse(
        native::wnd *owner, int x, int y, bool pressed, bool released) {
        bool used = false;
        for (auto *c : checks) {
            auto *b = check_bindings.object_from_handle(c);
            if (!b || root_of(c) != owner || !b->visible)
                continue;
            bool h = hit(root_bounds(*c), x, y);
            if (pressed) {
                b->pressed = h;
                used |= h;
            }
            if (released && b->pressed) {
                b->pressed = false;
                used = true;
                if (h) {
                    c->on_native_checked(!c->get_checked());
                    // User code may mutate the global check registry.
                    return true;
                }
            }
        }
        if (used)
            owner->invalidate();
        return used;
    }
    void render_checks(native::wnd *owner, native::gpx &g) {
        auto painter = native::theme::create(g);
        for (auto *c : checks) {
            auto *b = check_bindings.object_from_handle(c);
            if (!b || root_of(c) != owner || !b->visible)
                continue;
            native::theme::state s;
            s.hot = b->hover;
            s.pressed = b->pressed;
            native::detail::control_render_access::draw(
                *c, g, *painter, root_bounds(*c), s);
        }
    }
} // namespace linux::sdl2
namespace native
{
    void check::apply_text() {
        auto *b = linux::sdl2::check_bindings.object_from_handle(this);
        if (!b)
            throw std::runtime_error("SDL2: Missing check binding.");
        b->label = _text;
        if (b->parent)
            b->parent->invalidate();
    }
    void check::apply_checked() {
        auto *b = linux::sdl2::check_bindings.object_from_handle(this);
        if (!b)
            throw std::runtime_error("SDL2: Missing check binding.");
        b->checked = _checked;
        if (b->parent)
            b->parent->invalidate();
    }
    void check::create_native() {
        auto *p = get_parent();
        if (!p || !p->get_created())
            throw std::runtime_error(
                "SDL2: check requires a created parent.");
        auto *self = this;
        auto *b = new linux::sdl2::sdl2_check();
        b->parent = p;
        b->bounds = _bounds;
        b->label = _text;
        b->checked = _checked;
        linux::sdl2::check_bindings.register_pair(self, b);
        linux::sdl2::checks.push_back(self);
    }
    void check::show_native() {
        auto *b = linux::sdl2::check_bindings.object_from_handle(
            this);
        if (!_created || !b)
            throw std::runtime_error("SDL2: check is not created.");
        b->visible = true;
        if (b->parent)
            b->parent->invalidate();
    }
    void check::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *b = linux::sdl2::check_bindings.object_from_handle(self);
        auto &checks = linux::sdl2::checks;
        checks.erase(std::remove(checks.begin(), checks.end(), self),
                     checks.end());
        if (b && b->parent)
            b->parent->invalidate();
        linux::sdl2::check_bindings.unregister_by_handle(self);
        delete b;
    }
} // namespace native
