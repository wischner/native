//
// Provides reusable control composition for toolkits without a native
// painter. Concrete toolkit themes supply their own palette and text metrics.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <algorithm>

#include <native.h>

namespace linux
{
    class emulated_theme : public native::theme
    {
    public:
        explicit emulated_theme(native::gpx &g) : theme(g) {}

        theme &draw_button(
            const native::rect &r,
            const std::string &text,
            const state &s) override {
            saved_state saved(_g);
            const palette p = native_palette();
            const native::rgba background = s.pressed
                ? p.button_pressed_bg
                : (s.hot ? p.button_hot_bg : p.button_bg);
            const native::rgba foreground = s.disabled
                ? p.button_disabled_text
                : (s.pressed ? p.button_pressed_text
                             : (s.hot ? p.button_hot_text : p.button_text));
            const int offset = s.pressed ? 1 : 0;

            _g.set_pen(1).set_ink(background).draw_rect(r, true);
            draw_bevel(r, s.pressed, p);
            _g.set_font(native::font_t::stock(native::font_role::control));
            _g.set_ink(foreground).draw_text(
                text,
                native::point(
                    r.p.x + std::max(
                        0,
                        (static_cast<int>(r.d.w) - text_width(text)) / 2) + offset,
                    text_y(r) + offset));
            return *this;
        }

        theme &draw_menu_bar(const native::rect &r) override {
            saved_state saved(_g);
            const palette p = native_palette();
            _g.set_pen(1).set_ink(p.menu_bar_bg).draw_rect(r, true);
            if (r.d.w && r.d.h) {
                _g.set_ink(p.menu_bar_line_top).draw_line(
                    r.p, native::point(r.x2() - 1, r.p.y));
                _g.set_ink(p.menu_bar_line_bottom).draw_line(
                    native::point(r.p.x, r.y2() - 1),
                    native::point(r.x2() - 1, r.y2() - 1));
            }
            return *this;
        }

        theme &draw_menu_title(
            const native::rect &r,
            const std::string &text,
            const state &s) override {
            return draw_menu_entry(r, text, s, true);
        }

        theme &draw_menu_item(
            const native::rect &r,
            const std::string &text,
            const state &s) override {
            return draw_menu_entry(r, text, s, false);
        }

        theme &draw_popup_frame(const native::rect &r) override {
            saved_state saved(_g);
            const palette p = native_palette();
            _g.set_pen(1).set_ink(p.menu_popup_bg).draw_rect(r, true);
            _g.set_ink(p.menu_popup_border).draw_rect(r, false);
            return *this;
        }

        theme &draw_list_item(
            const native::rect &r,
            const std::string &text,
            const state &s) override {
            return draw_menu_entry(r, text, s, false);
        }

    protected:
        virtual int text_width(const std::string &text) const = 0;
        virtual int text_height() const = 0;
        virtual bool text_uses_baseline() const = 0;

        int text_y(const native::rect &r) const {
            if (text_uses_baseline())
                return r.p.y + (static_cast<int>(r.d.h) + text_height()) / 2;
            return r.p.y + std::max(
                0,
                (static_cast<int>(r.d.h) - text_height()) / 2);
        }

    private:
        class saved_state
        {
        public:
            explicit saved_state(native::gpx &g)
                : _g(g), _ink(g.get_ink()), _paper(g.get_paper()),
                  _pen(g.get_pen()), _font(g.get_font()), _clip(g.get_clip()) {}

            ~saved_state() {
                _g.set_ink(_ink)
                    .set_paper(_paper)
                    .set_pen(_pen)
                    .set_font(_font);
                _g.set_clip(_clip);
            }

        private:
            native::gpx &_g;
            native::rgba _ink;
            native::rgba _paper;
            std::uint8_t _pen;
            const native::font_t &_font;
            native::rect _clip;
        };

        void draw_bevel(
            const native::rect &r,
            bool inset,
            const palette &p) {
            if (!r.d.w || !r.d.h)
                return;
            const native::rgba top = inset
                ? p.button_shadow
                : p.button_highlight;
            const native::rgba bottom = inset
                ? p.button_highlight
                : p.button_shadow;
            _g.set_ink(top)
                .draw_line(r.p, native::point(r.x2() - 1, r.p.y))
                .draw_line(r.p, native::point(r.p.x, r.y2() - 1));
            _g.set_ink(bottom)
                .draw_line(native::point(r.p.x, r.y2() - 1),
                           native::point(r.x2() - 1, r.y2() - 1))
                .draw_line(native::point(r.x2() - 1, r.p.y),
                           native::point(r.x2() - 1, r.y2() - 1));
            _g.set_ink(p.button_border).draw_rect(r, false);
        }

        theme &draw_menu_entry(
            const native::rect &r,
            const std::string &text,
            const state &s,
            bool title) {
            saved_state saved(_g);
            const palette p = native_palette();
            const bool active = s.selected || s.hot;
            const native::rgba background = active
                ? p.menu_hot_bg
                : (title ? p.menu_bar_bg : p.menu_popup_bg);
            const native::rgba foreground = s.disabled
                ? p.menu_disabled_text
                : (active ? p.menu_hot_text : p.menu_text);

            _g.set_pen(1).set_ink(background).draw_rect(r, true);
            _g.set_font(native::font_t::stock(native::font_role::control));
            _g.set_ink(foreground).draw_text(
                text,
                native::point(
                    r.p.x + defaults().text_padding_x,
                    text_y(r)));
            return *this;
        }
    };
}
