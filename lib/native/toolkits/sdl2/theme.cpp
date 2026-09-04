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
            m.header_height = std::max(
                20,
                native::font_t::stock(native::font_role::control)
                        .get_metrics()
                        .height +
                    8);
            m.tab_height = m.header_height;
            m.disclosure_size = 9;
            m.tree_lines_visible = false;
            return m;
        }

        palette native_palette() const override {
            palette p;
            p.button_bg = native::rgba(212, 212, 212, 255);
            p.button_border = native::rgba(64, 64, 64, 255);
            p.button_highlight = native::rgba(255, 255, 255, 255);
            p.button_shadow = native::rgba(128, 128, 128, 255);
            p.button_text = native::rgba(32, 32, 32, 255);
            p.button_disabled_text = native::rgba(128, 128, 128, 255);
            p.button_hot_bg = native::rgba(228, 228, 228, 255);
            p.button_hot_text = p.button_text;
            p.button_pressed_bg = native::rgba(192, 192, 192, 255);
            p.button_pressed_text = p.button_text;
            p.menu_bar_bg = p.button_bg;
            p.menu_bar_line_top = native::rgba(255, 255, 255, 255);
            p.menu_bar_line_bottom = p.button_shadow;
            p.menu_text = p.button_text;
            p.menu_disabled_text = p.button_disabled_text;
            p.menu_hot_bg = native::rgba(96, 96, 96, 255);
            p.menu_hot_text = native::rgba(255, 255, 255, 255);
            p.menu_popup_bg = native::rgba(255, 255, 255, 255);
            p.menu_popup_border = p.button_border;
            p.content_bg = native::rgba(255, 255, 255, 255);
            p.content_alt_bg = native::rgba(246, 246, 246, 255);
            p.content_text = p.button_text;
            p.selection_bg = p.menu_hot_bg;
            p.selection_text = p.menu_hot_text;
            p.selection_inactive_bg = native::rgba(176, 176, 176, 255);
            p.selection_inactive_text = p.button_text;
            p.separator = p.menu_bar_line_bottom;
            p.focus = native::rgba(0, 0, 0, 255);
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
            const int extent = std::max(
                5, std::min(14, static_cast<int>(r.d.h) - 4));
            const native::rect indicator(
                static_cast<native::coord>(r.p.x + 2),
                static_cast<native::coord>(
                    r.p.y + (static_cast<int>(r.d.h) - extent) / 2),
                static_cast<native::dim>(extent),
                static_cast<native::dim>(extent));
            _g.set_pen(1).set_ink(p.button_bg).draw_rect(r, true)
                .set_ink(p.content_bg)
                .draw_rect(indicator, true);
            _g.set_ink(p.button_border).draw_rect(indicator, false);
            if (s.selected) {
                _g.set_pen(2)
                    .set_ink(s.disabled ? p.button_disabled_text
                                        : p.button_text)
                    .draw_line(
                        native::point(indicator.p.x + 3,
                                      indicator.p.y + extent / 2),
                        native::point(indicator.p.x + extent / 2,
                                      indicator.y2() - 3))
                    .draw_line(
                        native::point(indicator.p.x + extent / 2,
                                      indicator.y2() - 3),
                        native::point(indicator.x2() - 3,
                                      indicator.p.y + 3));
            }
            draw_selection_label(r, text, s, extent);
            draw_focus(r, s);
            return *this;
        }

        theme &draw_radio(const native::rect &r,
                          const std::string &text,
                          const state &s) override {
            state_guard guard(_g);
            const palette p = native_palette();
            const int extent = std::max(
                5, std::min(14, static_cast<int>(r.d.h) - 4));
            const native::rect indicator(
                static_cast<native::coord>(r.p.x + 2),
                static_cast<native::coord>(
                    r.p.y + (static_cast<int>(r.d.h) - extent) / 2),
                static_cast<native::dim>(extent),
                static_cast<native::dim>(extent));
            _g.set_pen(1).set_ink(p.button_bg).draw_rect(r, true)
                .set_ink(p.content_bg).draw_ellipse(indicator, true)
                .set_ink(p.button_border).draw_ellipse(indicator, false);
            if (s.selected) {
                const int inset = std::max(2, extent / 3);
                _g.set_ink(s.disabled ? p.button_disabled_text
                                      : p.button_text)
                    .draw_ellipse(
                        native::rect(
                            static_cast<native::coord>(
                                indicator.p.x + inset),
                            static_cast<native::coord>(
                                indicator.p.y + inset),
                            static_cast<native::dim>(
                                std::max(1, extent - inset * 2)),
                            static_cast<native::dim>(
                                std::max(1, extent - inset * 2))),
                        true);
            }
            draw_selection_label(r, text, s, extent);
            draw_focus(r, s);
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
        void draw_selection_label(const native::rect &r,
                                  const std::string &text,
                                  const state &s,
                                  int extent) {
            const palette p = native_palette();
            const int left = r.p.x + extent + 8;
            _g.set_font(
                native::font_t::stock(native::font_role::control));
            _g.set_ink(s.disabled ? p.button_disabled_text
                                  : p.button_text)
                .draw_text(
                    text,
                    native::rect(
                        static_cast<native::coord>(left), r.p.y,
                        static_cast<native::dim>(
                            std::max(0, r.x2() - left)), r.d.h),
                    {native::text_align::start,
                     native::text_valign::center,
                     native::text_overflow::ellipsis,
                     true});
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
