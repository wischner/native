#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

#include <native.h>

#include "globals.h"

namespace
{
    std::vector<native::button *> g_buttons;

    bool is_inside(const native::rect &r, int x, int y)
    {
        return x >= r.x1() && y >= r.y1() && x < r.x2() && y < r.y2();
    }
}

namespace sdl
{
    bool handle_button_motion(native::wnd *owner, int x, int y)
    {
        if (!owner)
            return false;

        bool changed = false;

        for (auto *btn : g_buttons)
        {
            auto *h = button_bindings.from_a(btn);
            if (!h || h->parent != owner || !h->visible)
                continue;

            h->bounds = btn->bounds();
            const bool now_hover = is_inside(h->bounds, x, y);
            if (now_hover != h->hover)
            {
                h->hover = now_hover;
                changed = true;
            }
        }

        if (changed)
            owner->invalidate();

        return changed;
    }

    bool handle_button_mouse(native::wnd *owner, int x, int y, bool pressed, bool released)
    {
        if (!owner)
            return false;

        bool consumed = false;

        for (auto *btn : g_buttons)
        {
            auto *h = button_bindings.from_a(btn);
            if (!h || h->parent != owner || !h->visible)
                continue;

            h->bounds = btn->bounds();

            if (pressed)
            {
                const bool hit = is_inside(h->bounds, x, y);
                h->hover = hit;
                if (hit)
                {
                    h->pressed = true;
                    consumed = true;
                }
                else
                {
                    h->pressed = false;
                }
            }

            if (released)
            {
                const bool was_pressed = h->pressed;
                h->pressed = false;
                h->hover = is_inside(h->bounds, x, y);

                if (was_pressed)
                {
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

    void render_buttons(native::wnd *owner, native::gpx &g)
    {
        if (!owner)
            return;

        native::control_paint cp(g);
        for (auto *btn : g_buttons)
        {
            auto *h = button_bindings.from_a(btn);
            if (!h || h->parent != owner || !h->visible)
                continue;

            h->bounds = btn->bounds();

            native::control_paint::state st;
            st.hot = h->hover;
            st.pressed = h->pressed;
            cp.draw_button(h->bounds, h->label, st);
        }
    }
} // namespace sdl

namespace native
{
    button::button(std::string text, coord x, coord y, dim w, dim h)
        : wnd(x, y, w, h), _text(std::move(text))
    {
    }

    button::button(const std::string &text, const point &pos, const size &dim)
        : button(text, pos.x, pos.y, dim.w, dim.h)
    {
    }

    button::button(const std::string &text, const rect &bounds)
        : button(text, bounds.p.x, bounds.p.y, bounds.d.w, bounds.d.h)
    {
    }

    const std::string &button::text() const
    {
        return _text;
    }

    button &button::set_text(const std::string &text)
    {
        _text = text;

        if (_created)
        {
            auto *h = sdl::button_bindings.from_a(const_cast<button *>(this));
            if (h)
            {
                h->label = _text;
                if (h->parent)
                    h->parent->invalidate();
            }
        }

        return *this;
    }

    void button::create() const
    {
        if (_created)
            return;

        wnd *p = parent();
        if (!p)
            throw std::runtime_error("SDL2: button requires a parent window.");

        auto *self = const_cast<button *>(this);

        auto *h = new sdl::sdl2button();
        h->owner = self;
        h->parent = p;
        h->bounds = bounds();
        h->label = _text;
        h->hover = false;
        h->pressed = false;
        h->visible = false;
        sdl::button_bindings.register_pair(self, h);

        g_buttons.push_back(self);

        _created = true;
        self->on_wnd_create.emit();
    }

    void button::show() const
    {
        if (!_created)
            throw std::runtime_error("SDL2: Cannot show button before it is created.");

        auto *h = sdl::button_bindings.from_a(const_cast<button *>(this));
        if (!h)
            throw std::runtime_error("SDL2: Missing button binding.");

        h->visible = true;
        h->hover = false;
        h->pressed = false;
        if (h->parent)
            h->parent->invalidate();
    }

    void button::destroy() const
    {
        if (!_created)
            return;

        auto *self = const_cast<button *>(this);
        auto *h = sdl::button_bindings.from_a(self);

        if (h)
        {
            auto it = std::remove(g_buttons.begin(), g_buttons.end(), self);
            g_buttons.erase(it, g_buttons.end());

            if (h->parent)
                h->parent->invalidate();

            sdl::button_bindings.unregister_by_a(self);
            delete h;
        }

        _created = false;
    }
} // namespace native
