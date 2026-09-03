//
// Implements the SDL-emulated radio control.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <algorithm>
#include <stdexcept>
#include <native.h>
#include <native/radio.h>
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
    bool handle_radio_motion(native::wnd *owner, int x, int y) {
        bool changed = false;
        for (auto *c : radios) {
            auto *b = radio_bindings.object_from_handle(c);
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
    bool handle_radio_mouse(
        native::wnd *owner, int x, int y, bool pressed, bool released) {
        bool used = false;
        for (auto *c : radios) {
            auto *b = radio_bindings.object_from_handle(c);
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
                if (h)
                    c->on_native_selected();
            }
        }
        if (used)
            owner->invalidate();
        return used;
    }
    void render_radios(native::wnd *owner, native::gpx &g) {
        auto painter = native::theme::create(g);
        for (auto *c : radios) {
            auto *b = radio_bindings.object_from_handle(c);
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
    void radio::apply_text() {
        auto *b = linux::sdl2::radio_bindings.object_from_handle(this);
        if (!b)
            throw std::runtime_error("SDL2: Missing radio binding.");
        b->label = _text;
        if (b->parent)
            b->parent->invalidate();
    }
    void radio::apply_selected() {
        auto *b = linux::sdl2::radio_bindings.object_from_handle(this);
        if (!b)
            throw std::runtime_error("SDL2: Missing radio binding.");
        b->selected = _selected;
        if (b->parent)
            b->parent->invalidate();
    }
    void radio::create() const {
        if (_created)
            return;
        auto *p = get_parent();
        if (!p || !p->get_created())
            throw std::runtime_error(
                "SDL2: radio requires a created parent.");
        auto *self = const_cast<radio *>(this);
        auto *b = new linux::sdl2::sdl2_radio();
        b->parent = p;
        b->bounds = _bounds;
        b->label = _text;
        b->selected = _selected;
        linux::sdl2::radio_bindings.register_pair(self, b);
        linux::sdl2::radios.push_back(self);
        _created = true;
        self->on_native_create();
    }
    void radio::show() const {
        auto *b = linux::sdl2::radio_bindings.object_from_handle(
            const_cast<radio *>(this));
        if (!_created || !b)
            throw std::runtime_error("SDL2: radio is not created.");
        b->visible = true;
        if (b->parent)
            b->parent->invalidate();
    }
    void radio::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<radio *>(this);
        auto *b = linux::sdl2::radio_bindings.object_from_handle(self);
        self->on_native_destroy();
        auto &radios = linux::sdl2::radios;
        radios.erase(std::remove(radios.begin(), radios.end(), self),
                     radios.end());
        if (b && b->parent)
            b->parent->invalidate();
        linux::sdl2::radio_bindings.unregister_by_handle(self);
        delete b;
    }
} // namespace native
