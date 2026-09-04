//
// Implements the SDL-emulated list control.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <algorithm>
#include <stdexcept>
#include <native.h>
#include <native/list.h>
#include "../../control_render_access.h"
#include "globals.h"
namespace linux::sdl2
{
    bool
    handle_list_mouse(native::wnd *owner, int x, int y, bool released) {
        if (!released)
            return false;
        for (auto *c : lists) {
            auto *b = list_bindings.object_from_handle(c);
            auto r = root_bounds(*c);
            if (!b || root_of(c) != owner || !b->visible ||
                !r.contains(native::point(x, y)))
                continue;
            int index = (y - r.p.y - 1) / 20;
            if (index >= 0 &&
                index < static_cast<int>(c->get_items().size()))
                c->on_native_selection(index);
            // Selection callbacks may close the owner or mutate the list
            // registry. The event loop invalidates the owner if it survives.
            return true;
        }
        return false;
    }
    void render_lists(native::wnd *owner, native::gpx &g) {
        auto painter = native::theme::create(g);
        for (auto *c : lists) {
            auto *b = list_bindings.object_from_handle(c);
            if (b && root_of(c) == owner && b->visible)
                native::detail::control_render_access::draw(
                    *c,
                    g,
                    *painter,
                    root_bounds(*c),
                    native::theme::state{});
        }
    }
} // namespace linux::sdl2
namespace native
{
    void list::apply_items() {
        auto *b = linux::sdl2::list_bindings.object_from_handle(this);
        if (!b)
            throw std::runtime_error("SDL2: Missing list binding.");
        b->items = _items;
        if (b->parent)
            b->parent->invalidate();
    }
    void list::apply_selected_index() {
        auto *b = linux::sdl2::list_bindings.object_from_handle(this);
        if (!b)
            throw std::runtime_error("SDL2: Missing list binding.");
        b->selected_index = _selected_index;
        if (b->parent)
            b->parent->invalidate();
    }
    void list::create_native() {
        auto *p = get_parent();
        if (!p || !p->get_created())
            throw std::runtime_error(
                "SDL2: list requires a created parent.");
        auto *self = this;
        auto *b = new linux::sdl2::sdl2_list();
        b->parent = p;
        b->bounds = _bounds;
        b->items = _items;
        b->selected_index = _selected_index;
        linux::sdl2::list_bindings.register_pair(self, b);
        linux::sdl2::lists.push_back(self);
    }
    void list::show_native() {
        auto *b = linux::sdl2::list_bindings.object_from_handle(
            this);
        if (!_created || !b)
            throw std::runtime_error("SDL2: list is not created.");
        b->visible = true;
        if (b->parent)
            b->parent->invalidate();
    }
    void list::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *b = linux::sdl2::list_bindings.object_from_handle(self);
        auto &lists = linux::sdl2::lists;
        lists.erase(std::remove(lists.begin(), lists.end(), self),
                    lists.end());
        if (b && b->parent)
            b->parent->invalidate();
        linux::sdl2::list_bindings.unregister_by_handle(self);
        delete b;
    }
} // namespace native
