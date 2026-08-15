//
// Implements the SDL2 button-control backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

#include <native.h>

#include "globals.h"

namespace
{
    std::vector<native::button *> g_buttons;

    bool is_inside(const native::rect &r, int x, int y) {
        return x >= r.x1() && y >= r.y1() && x < r.x2() && y < r.y2();
    }
}

namespace linux::sdl2
{
    bool handle_button_motion(native::wnd *owner, int x, int y) {
        if (!owner)
            return false;

        bool changed = false;

        for (auto *btn : g_buttons) {
            auto *h = button_bindings.object_from_handle(btn);
            if (!h || h->parent != owner || !h->visible)
                continue;

            h->bounds = btn->get_bounds();
            const bool now_hover = is_inside(h->bounds, x, y);
            if (now_hover != h->hover) {
                h->hover = now_hover;
                changed = true;
            }
        }

        if (changed)
            owner->invalidate();

        return changed;
    }

    bool handle_button_mouse(native::wnd *owner, int x, int y, bool pressed, bool released) {
        if (!owner)
            return false;

        bool consumed = false;

        for (auto *btn : g_buttons) {
            auto *h = button_bindings.object_from_handle(btn);
            if (!h || h->parent != owner || !h->visible)
                continue;

            h->bounds = btn->get_bounds();

            if (pressed) {
                const bool hit = is_inside(h->bounds, x, y);
                h->hover = hit;
                if (hit) {
                    h->pressed = true;
                    consumed = true;
                }
                else {
                    h->pressed = false;
                }
            }

            if (released) {
                const bool was_pressed = h->pressed;
                h->pressed = false;
                h->hover = is_inside(h->bounds, x, y);

                if (was_pressed) {
                    consumed = true;
                    if (is_inside(h->bounds, x, y))
                        btn->on_click.emit();
                }
            }
        }

        if (consumed)
            owner->invalidate();

        return consumed;
    }

    void render_buttons(native::wnd *owner, native::gpx &g) {
        if (!owner)
            return;

        auto painter = native::theme::create(g);
        for (auto *btn : g_buttons) {
            auto *h = button_bindings.object_from_handle(btn);
            if (!h || h->parent != owner || !h->visible)
                continue;

            h->bounds = btn->get_bounds();

            native::theme::state st;
            st.hot = h->hover;
            st.pressed = h->pressed;
            painter->draw_button(h->bounds, h->label, st);
        }
    }
} // namespace linux::sdl2

namespace native
{
    void button::apply_text() {
        auto *binding =
            linux::sdl2::button_bindings.object_from_handle(this);
        if (!binding)
            throw std::runtime_error(
                "SDL2: Missing button binding.");

        binding->label = _text;
        if (binding->parent)
            binding->parent->invalidate();
    }

    void button::create() const {
        if (_created)
            return;

        wnd *p = get_parent();
        if (!p)
            throw std::runtime_error("SDL2: button requires a parent window.");
        if (!p->get_created())
            throw std::runtime_error(
                "SDL2: button parent is not created.");

        auto *self = const_cast<button *>(this);

        auto *h = new linux::sdl2::sdl2_button();
        h->owner = self;
        h->parent = p;
        h->bounds = get_bounds();
        h->label = _text;
        h->hover = false;
        h->pressed = false;
        h->visible = false;
        linux::sdl2::button_bindings.register_pair(self, h);

        g_buttons.push_back(self);

        _created = true;
        self->on_wnd_create.emit();
    }

    void button::show() const {
        if (!_created)
            throw std::runtime_error("SDL2: Cannot show button before it is created.");

        auto *h = linux::sdl2::button_bindings.object_from_handle(const_cast<button *>(this));
        if (!h)
            throw std::runtime_error("SDL2: Missing button binding.");

        h->visible = true;
        h->hover = false;
        h->pressed = false;
        if (h->parent)
            h->parent->invalidate();
    }

    void button::destroy() const {
        if (!_created)
            return;

        auto *self = const_cast<button *>(this);
        auto *h = linux::sdl2::button_bindings.object_from_handle(self);
        self->on_native_destroy();

        if (h) {
            auto it = std::remove(g_buttons.begin(), g_buttons.end(), self);
            g_buttons.erase(it, g_buttons.end());

            if (h->parent)
                h->parent->invalidate();

            linux::sdl2::button_bindings.unregister_by_handle(self);
            delete h;
        }
    }
} // namespace native
