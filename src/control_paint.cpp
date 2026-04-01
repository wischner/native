#include <algorithm>

#include <native.h>

#include "control_paint_backend.h"

namespace native
{
    control_paint::control_paint(gpx &painter)
        : _g(painter)
    {
    }

    control_paint::metrics control_paint::defaults() const
    {
        return detail::control_paint_backend_metrics();
    }

    control_paint::palette control_paint::native_palette()
    {
        return detail::control_paint_backend_palette();
    }

    control_paint &control_paint::draw_button_face(const rect &r, const state &s)
    {
        if (detail::control_paint_backend_draw_button_face_native(_g, r, s))
            return *this;

        const palette p = native_palette();
        const rgba old_ink = _g.ink();
        const uint8_t old_pen = _g.pen();

        const rgba bg = s.pressed ? p.button_pressed_bg : (s.hot ? p.button_hot_bg : p.button_bg);
        _g.set_pen(1);
        _g.set_ink(bg).draw_rect(r, true);

        _g.set_pen(old_pen).set_ink(old_ink);
        return *this;
    }

    control_paint &control_paint::draw_button_frame(const rect &r, const state &s)
    {
        if (detail::control_paint_backend_draw_button_frame_native(_g, r, s))
            return *this;

        const palette p = native_palette();
        const rgba old_ink = _g.ink();
        const uint8_t old_pen = _g.pen();

        _g.set_pen(1);
        _g.set_ink(p.button_border).draw_rect(r, false);

        _g.set_pen(old_pen).set_ink(old_ink);
        return *this;
    }

    control_paint &control_paint::draw_button_text(const rect &r, const std::string &text, const state &s)
    {
        if (detail::control_paint_backend_draw_button_text_native(_g, r, text, s))
            return *this;

        const palette p = native_palette();
        const rgba old_ink = _g.ink();
        const uint8_t old_pen = _g.pen();
        const font_t &old_font = _g.font();

        const rgba fg = s.pressed ? p.button_pressed_text : (s.hot ? p.button_hot_text : p.button_text);
        const int tw = detail::control_paint_backend_text_width(text);
        const int tx = r.p.x + std::max(0, (static_cast<int>(r.d.w) - tw) / 2);
        const int ty = detail::control_paint_backend_text_y_centered(r) + (s.pressed ? 1 : 0);

        _g.set_font(font_t::stock(font_role::control));
        _g.set_ink(fg).draw_text(text, point(tx + (s.pressed ? 1 : 0), ty));

        _g.set_font(old_font).set_pen(old_pen).set_ink(old_ink);
        return *this;
    }

    control_paint &control_paint::draw_button(const rect &r, const std::string &text)
    {
        return draw_button(r, text, state{});
    }

    control_paint &control_paint::draw_button(const rect &r, const std::string &text, const state &s)
    {
        draw_button_face(r, s);
        draw_button_frame(r, s);
        draw_button_text(r, text, s);
        return *this;
    }

    control_paint &control_paint::draw_menu_bar_background(const rect &r)
    {
        if (detail::control_paint_backend_draw_menu_bar_background_native(_g, r))
            return *this;

        const palette p = native_palette();
        const rgba old_ink = _g.ink();
        const uint8_t old_pen = _g.pen();

        _g.set_pen(1);
        _g.set_ink(p.menu_bar_bg).draw_rect(r, true);

        _g.set_pen(old_pen).set_ink(old_ink);
        return *this;
    }

    control_paint &control_paint::draw_menu_item_background(const rect &r, const state &s)
    {
        if (detail::control_paint_backend_draw_menu_item_background_native(_g, r, s))
            return *this;

        const palette p = native_palette();
        const rgba old_ink = _g.ink();
        const uint8_t old_pen = _g.pen();
        const bool active = s.selected || s.hot;

        _g.set_pen(1);
        _g.set_ink(active ? p.menu_hot_bg : p.menu_popup_bg).draw_rect(r, true);

        _g.set_pen(old_pen).set_ink(old_ink);
        return *this;
    }

    control_paint &control_paint::draw_menu_item_text(const rect &r, const std::string &text, const state &s)
    {
        if (detail::control_paint_backend_draw_menu_item_text_native(_g, r, text, s))
            return *this;

        const palette p = native_palette();
        const metrics m = defaults();
        const rgba old_ink = _g.ink();
        const uint8_t old_pen = _g.pen();
        const font_t &old_font = _g.font();
        const bool active = s.selected || s.hot;

        _g.set_font(font_t::stock(font_role::control));
        _g.set_ink(active ? p.menu_hot_text : p.menu_text);
        _g.draw_text(text, point(r.p.x + m.text_padding_x, detail::control_paint_backend_text_y_centered(r)));

        _g.set_font(old_font).set_pen(old_pen).set_ink(old_ink);
        return *this;
    }

    control_paint &control_paint::draw_menu_bar(const rect &r)
    {
        const palette p = native_palette();
        const rgba old_ink = _g.ink();
        const uint8_t old_pen = _g.pen();
        draw_menu_bar_background(r);
        _g.set_pen(1);
        _g.set_ink(p.menu_bar_line_top).draw_line(point(r.x1(), r.y1()), point(r.x2(), r.y1()));
        _g.set_ink(p.menu_bar_line_bottom).draw_line(point(r.x1(), r.y2()), point(r.x2(), r.y2()));
        _g.set_pen(old_pen).set_ink(old_ink);
        return *this;
    }

    control_paint &control_paint::draw_menu_title(const rect &r, const std::string &text)
    {
        return draw_menu_title(r, text, state{});
    }

    control_paint &control_paint::draw_menu_title(const rect &r, const std::string &text, const state &s)
    {
        draw_menu_item_background(r, s);
        draw_menu_item_text(r, text, s);
        return *this;
    }

    control_paint &control_paint::draw_menu_item(const rect &r, const std::string &text)
    {
        return draw_menu_item(r, text, state{});
    }

    control_paint &control_paint::draw_menu_item(const rect &r, const std::string &text, const state &s)
    {
        draw_menu_item_background(r, s);
        draw_menu_item_text(r, text, s);
        return *this;
    }

    control_paint &control_paint::draw_popup_frame(const rect &r)
    {
        const palette p = native_palette();
        const rgba old_ink = _g.ink();
        const uint8_t old_pen = _g.pen();

        _g.set_pen(1);
        _g.set_ink(p.menu_popup_bg).draw_rect(r, true);
        _g.set_ink(p.menu_popup_border).draw_rect(r, false);

        _g.set_pen(old_pen).set_ink(old_ink);
        return *this;
    }
} // namespace native
