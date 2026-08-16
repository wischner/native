//
// Implements the SDL2 theme with an SDL-native emulation.
// SDL has no standard desktop-control painter, so this backend owns the
// complete look instead of relying on shared control-rendering code.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <memory>

#include <native.h>
#include <native/theme.h>

#include "globals.h"

namespace
{
    class state_guard
    {
    public:
        explicit state_guard(native::gpx &g)
            : _g(g)
            , _ink(g.get_ink())
            , _paper(g.get_paper())
            , _pen(g.get_pen())
            , _font(g.get_font())
            , _clip(g.get_clip()) {}

        ~state_guard() {
            _g.set_ink(_ink).set_paper(_paper).set_pen(_pen).set_font(
                _font);
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

    class sdl_theme final : public native::theme
    {
    public:
        explicit sdl_theme(native::gpx &g)
            : theme(g) {}

        metrics defaults() const override {
            metrics m;
            m.menu_bar_height = 24;
            m.menu_item_height = 20;
            m.popup_width = 180;
            m.text_padding_x = 8;
            return m;
        }

        palette native_palette() const override {
            palette p;
            p.button_bg = native::rgba(245, 247, 250, 255);
            p.button_border = native::rgba(92, 99, 112, 255);
            p.button_highlight = native::rgba(255, 255, 255, 255);
            p.button_shadow = native::rgba(152, 160, 172, 255);
            p.button_text = native::rgba(32, 37, 43, 255);
            p.button_disabled_text = native::rgba(142, 147, 155, 255);
            p.button_hot_bg = native::rgba(230, 239, 255, 255);
            p.button_hot_text = p.button_text;
            p.button_pressed_bg = native::rgba(30, 108, 203, 255);
            p.button_pressed_text = native::rgba(255, 255, 255, 255);
            p.menu_bar_bg = native::rgba(245, 247, 250, 255);
            p.menu_bar_line_top = native::rgba(255, 255, 255, 255);
            p.menu_bar_line_bottom = native::rgba(195, 201, 210, 255);
            p.menu_text = p.button_text;
            p.menu_disabled_text = p.button_disabled_text;
            p.menu_hot_bg = native::rgba(26, 115, 232, 255);
            p.menu_hot_text = native::rgba(255, 255, 255, 255);
            p.menu_popup_bg = native::rgba(255, 255, 255, 255);
            p.menu_popup_border = native::rgba(120, 126, 136, 255);
            return p;
        }

        theme &draw_button(const native::rect &r,
                           const std::string &text,
                           const state &s) override {
            state_guard guard(_g);
            const palette p = native_palette();
            const native::rgba background =
                s.pressed ? p.button_pressed_bg
                          : (s.hot ? p.button_hot_bg : p.button_bg);
            const native::rgba foreground =
                s.disabled ? p.button_disabled_text
                           : (s.pressed ? p.button_pressed_text
                                        : (s.hot ? p.button_hot_text
                                                 : p.button_text));

            _g.set_pen(1).set_ink(background).draw_rect(r, true);
            draw_bevel(r, s.pressed, p);

            const int width = linux::sdl2::text_width(text);
            const int height = linux::sdl2::text_height();
            const int offset = s.pressed ? 1 : 0;
            _g.set_font(
                native::font_t::stock(native::font_role::control));
            _g.set_ink(foreground)
                .draw_text(
                    text,
                    native::point(
                        r.p.x +
                            std::max(0,
                                     (static_cast<int>(r.d.w) - width) /
                                         2) +
                            offset,
                        r.p.y +
                            std::max(
                                0,
                                (static_cast<int>(r.d.h) - height) /
                                    2) +
                            offset));
            return *this;
        }

        theme &draw_menu_bar(const native::rect &r) override {
            state_guard guard(_g);
            const palette p = native_palette();
            _g.set_pen(1).set_ink(p.menu_bar_bg).draw_rect(r, true);
            if (r.d.w && r.d.h) {
                _g.set_ink(p.menu_bar_line_top)
                    .draw_line(r.p, native::point(r.x2() - 1, r.p.y));
                _g.set_ink(p.menu_bar_line_bottom)
                    .draw_line(native::point(r.p.x, r.y2() - 1),
                               native::point(r.x2() - 1, r.y2() - 1));
            }
            return *this;
        }

        theme &draw_menu_title(const native::rect &r,
                               const std::string &text,
                               const state &s) override {
            return draw_menu_entry(r, text, s, true);
        }

        theme &draw_menu_item(const native::rect &r,
                              const std::string &text,
                              const state &s) override {
            return draw_menu_entry(r, text, s, false);
        }

        theme &draw_popup_frame(const native::rect &r) override {
            state_guard guard(_g);
            const palette p = native_palette();
            _g.set_pen(1).set_ink(p.menu_popup_bg).draw_rect(r, true);
            _g.set_ink(p.menu_popup_border).draw_rect(r, false);
            return *this;
        }

        theme &draw_list_item(const native::rect &r,
                              const std::string &text,
                              const state &s) override {
            return draw_menu_entry(r, text, s, false);
        }

        theme &draw_check(const native::rect &r,
                          const std::string &text,
                          const state &s) override {
            state_guard guard(_g);
            const palette p = native_palette();
            const native::rect indicator = indicator_bounds(r);
            _g.set_pen(1)
                .set_ink(p.button_bg)
                .draw_rect(indicator, true);
            draw_bevel(indicator, s.pressed, p);
            if (s.selected && indicator.d.w >= 7) {
                _g.set_pen(2)
                    .set_ink(s.disabled ? p.button_disabled_text
                                        : p.button_text)
                    .draw_line(native::point(indicator.p.x + 3,
                                             indicator.p.y +
                                                 indicator.d.h / 2),
                               native::point(indicator.p.x +
                                                 indicator.d.w / 2 - 1,
                                             indicator.y2() - 4))
                    .draw_line(native::point(indicator.p.x +
                                                 indicator.d.w / 2 - 1,
                                             indicator.y2() - 4),
                               native::point(indicator.x2() - 3,
                                             indicator.p.y + 3));
            }
            draw_selection_label(r, indicator, text, s);
            return *this;
        }

        theme &draw_radio(const native::rect &r,
                          const std::string &text,
                          const state &s) override {
            state_guard guard(_g);
            const palette p = native_palette();
            const native::rect indicator = indicator_bounds(r);
            const int right = indicator.x2() - 1;
            const int bottom = indicator.y2() - 1;
            _g.set_pen(1)
                .set_ink(p.button_bg)
                .draw_rect(native::rect(indicator.p.x + 2,
                                        indicator.p.y,
                                        indicator.d.w - 4,
                                        indicator.d.h),
                           true)
                .draw_rect(native::rect(indicator.p.x,
                                        indicator.p.y + 2,
                                        indicator.d.w,
                                        indicator.d.h - 4),
                           true)
                .set_ink(p.button_border)
                .draw_line(
                    native::point(indicator.p.x + 2, indicator.p.y),
                    native::point(right - 2, indicator.p.y))
                .draw_line(native::point(right, indicator.p.y + 2),
                           native::point(right, bottom - 2))
                .draw_line(native::point(right - 2, bottom),
                           native::point(indicator.p.x + 2, bottom))
                .draw_line(
                    native::point(indicator.p.x, bottom - 2),
                    native::point(indicator.p.x, indicator.p.y + 2));
            if (s.selected) {
                const int inset =
                    std::max(3, static_cast<int>(indicator.d.w) / 4);
                _g.set_ink(s.disabled ? p.button_disabled_text
                                      : p.button_text)
                    .draw_rect(native::rect(indicator.p.x + inset,
                                            indicator.p.y + inset,
                                            indicator.d.w - inset * 2,
                                            indicator.d.h - inset * 2),
                               true);
            }
            draw_selection_label(r, indicator, text, s);
            return *this;
        }

        theme &draw_list(const native::rect &r,
                         const std::vector<std::string> &items,
                         int selected_index,
                         const state &s) override {
            state_guard guard(_g);
            const palette p = native_palette();
            _g.set_pen(1).set_ink(p.menu_popup_bg).draw_rect(r, true);
            draw_bevel(r, true, p);
            if (r.d.w <= 2 || r.d.h <= 2)
                return *this;

            const native::rect content(
                r.p.x + 1, r.p.y + 1, r.d.w - 2, r.d.h - 2);
            _g.set_clip(_g.get_clip().intersect(content));
            const int item_height =
                std::max(1, defaults().list_item_height);
            for (std::size_t i = 0; i < items.size(); ++i) {
                const int y =
                    content.p.y + static_cast<int>(i) * item_height;
                if (y >= content.y2())
                    break;
                state item_state = s;
                item_state.selected =
                    static_cast<int>(i) == selected_index;
                draw_list_item(native::rect(content.p.x,
                                            y,
                                            content.d.w,
                                            std::min(item_height,
                                                     content.y2() - y)),
                               items[i],
                               item_state);
            }
            return *this;
        }

        theme &draw_text_edit_frame(
            const native::rect &r,
            const state &s) override {
            state_guard guard(_g);
            const palette p = native_palette();
            _g.set_pen(1)
                .set_ink(s.disabled ? p.button_bg : p.menu_popup_bg)
                .draw_rect(r, true);
            draw_bevel(r, true, p);
            return *this;
        }

    private:
        native::rect indicator_bounds(const native::rect &r) const {
            const int side =
                std::max(7, std::min(15, static_cast<int>(r.d.h) - 4));
            return native::rect(
                r.p.x + 2,
                r.p.y +
                    std::max(0, (static_cast<int>(r.d.h) - side) / 2),
                side,
                side);
        }

        void draw_selection_label(const native::rect &r,
                                  const native::rect &indicator,
                                  const std::string &text,
                                  const state &s) {
            const palette p = native_palette();
            _g.set_font(
                native::font_t::stock(native::font_role::control));
            _g.set_ink(s.disabled ? p.button_disabled_text
                                  : p.button_text)
                .draw_text(
                    text,
                    native::point(
                        indicator.x2() + 5,
                        r.p.y + std::max(0,
                                         (static_cast<int>(r.d.h) -
                                          linux::sdl2::text_height()) /
                                             2)));
        }

        void draw_bevel(const native::rect &r,
                        bool inset,
                        const palette &p) {
            if (!r.d.w || !r.d.h)
                return;
            const native::rgba top =
                inset ? p.button_shadow : p.button_highlight;
            const native::rgba bottom =
                inset ? p.button_highlight : p.button_shadow;
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

        theme &draw_menu_entry(const native::rect &r,
                               const std::string &text,
                               const state &s,
                               bool title) {
            state_guard guard(_g);
            const palette p = native_palette();
            const bool active = s.selected || s.hot;
            const native::rgba background =
                active ? p.menu_hot_bg
                       : (title ? p.menu_bar_bg : p.menu_popup_bg);
            const native::rgba foreground =
                s.disabled ? p.menu_disabled_text
                           : (active ? p.menu_hot_text : p.menu_text);
            const metrics m = defaults();
            const int height = linux::sdl2::text_height();

            _g.set_pen(1).set_ink(background).draw_rect(r, true);
            _g.set_font(
                native::font_t::stock(native::font_role::control));
            _g.set_ink(foreground)
                .draw_text(
                    text,
                    native::point(
                        r.p.x + m.text_padding_x,
                        r.p.y + std::max(
                                    0,
                                    (static_cast<int>(r.d.h) - height) /
                                        2)));
            return *this;
        }
    };
} // namespace

namespace native
{
    std::unique_ptr<theme> theme::create(gpx &painter) {
        return std::make_unique<sdl_theme>(painter);
    }
} // namespace native
