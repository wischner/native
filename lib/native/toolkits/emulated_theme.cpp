//
// Implements portable control composition for Linux toolkits without a
// native painter. Palette and text metrics remain backend-specific.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "emulated_theme.h"

#include <algorithm>

namespace linux
{
    emulated_theme::emulated_theme(native::gpx &graphics)
        : theme(graphics) {}

    native::theme &
    emulated_theme::draw_button(const native::rect &bounds,
                                const std::string &text,
                                const state &element_state) {
        saved_state saved(_g);
        const palette colors = native_palette();
        const native::rgba background =
            element_state.pressed
                ? colors.button_pressed_bg
                : (element_state.hot ? colors.button_hot_bg
                                     : colors.button_bg);
        const native::rgba foreground =
            element_state.disabled
                ? colors.button_disabled_text
                : (element_state.pressed
                       ? colors.button_pressed_text
                       : (element_state.hot ? colors.button_hot_text
                                            : colors.button_text));
        const int offset = element_state.pressed ? 1 : 0;

        _g.set_pen(1).set_ink(background).draw_rect(bounds, true);
        draw_bevel(bounds, element_state.pressed, colors);
        _g.set_font(native::font_t::stock(native::font_role::control));
        _g.set_clip(_g.get_clip().intersect(bounds));
        _g.set_ink(foreground)
            .draw_text(text,
                       native::point(
                           bounds.p.x +
                               std::max(0,
                                        (static_cast<int>(bounds.d.w) -
                                         text_width(text)) /
                                            2) +
                               offset,
                           text_y(bounds) + offset));
        return *this;
    }

    native::theme &
    emulated_theme::draw_menu_bar(const native::rect &bounds) {
        saved_state saved(_g);
        const palette colors = native_palette();
        _g.set_pen(1)
            .set_ink(colors.menu_bar_bg)
            .draw_rect(bounds, true);
        if (bounds.d.w && bounds.d.h) {
            _g.set_ink(colors.menu_bar_line_top)
                .draw_line(bounds.p,
                           native::point(bounds.x2() - 1, bounds.p.y));
            _g.set_ink(colors.menu_bar_line_bottom)
                .draw_line(
                    native::point(bounds.p.x, bounds.y2() - 1),
                    native::point(bounds.x2() - 1, bounds.y2() - 1));
        }
        return *this;
    }

    native::theme &
    emulated_theme::draw_menu_title(const native::rect &bounds,
                                    const std::string &text,
                                    const state &element_state) {
        return draw_menu_entry(bounds, text, element_state, true);
    }

    native::theme &
    emulated_theme::draw_menu_item(const native::rect &bounds,
                                   const std::string &text,
                                   const state &element_state) {
        return draw_menu_entry(bounds, text, element_state, false);
    }

    native::theme &
    emulated_theme::draw_popup_frame(const native::rect &bounds) {
        saved_state saved(_g);
        const palette colors = native_palette();
        _g.set_pen(1)
            .set_ink(colors.menu_popup_bg)
            .draw_rect(bounds, true);
        _g.set_ink(colors.menu_popup_border).draw_rect(bounds, false);
        return *this;
    }

    native::theme &
    emulated_theme::draw_list_item(const native::rect &bounds,
                                   const std::string &text,
                                   const state &element_state) {
        return draw_menu_entry(bounds, text, element_state, false);
    }

    native::theme &
    emulated_theme::draw_check(const native::rect &bounds,
                               const std::string &text,
                               const state &element_state) {
        saved_state saved(_g);
        const palette colors = native_palette();
        const int side =
            std::max(5, std::min(15, static_cast<int>(bounds.d.h) - 4));
        const native::rect indicator(
            bounds.p.x + 2,
            bounds.p.y +
                std::max(0, (static_cast<int>(bounds.d.h) - side) / 2),
            static_cast<native::dim>(side),
            static_cast<native::dim>(side));
        draw_indicator_box(indicator, element_state.pressed, colors);
        if (element_state.selected && side >= 7) {
            _g.set_pen(2)
                .set_ink(element_state.disabled
                             ? colors.button_disabled_text
                             : colors.button_text)
                .draw_line(native::point(indicator.p.x + 3,
                                         indicator.p.y + side / 2),
                           native::point(indicator.p.x + side / 2 - 1,
                                         indicator.y2() - 4))
                .draw_line(native::point(indicator.p.x + side / 2 - 1,
                                         indicator.y2() - 4),
                           native::point(indicator.x2() - 3,
                                         indicator.p.y + 3));
        }
        draw_control_label(
            bounds, indicator.x2() + 5, text, element_state, colors);
        return *this;
    }

    native::theme &
    emulated_theme::draw_radio(const native::rect &bounds,
                               const std::string &text,
                               const state &element_state) {
        saved_state saved(_g);
        const palette colors = native_palette();
        const int side =
            std::max(7, std::min(15, static_cast<int>(bounds.d.h) - 4));
        const int x = bounds.p.x + 2;
        const int y =
            bounds.p.y +
            std::max(0, (static_cast<int>(bounds.d.h) - side) / 2);
        const int right = x + side - 1;
        const int bottom = y + side - 1;
        const int inset = std::max(2, side / 4);
        _g.set_pen(1).set_ink(colors.button_bg);
        _g.draw_rect(native::rect(static_cast<native::coord>(x + 2),
                                  static_cast<native::coord>(y),
                                  static_cast<native::dim>(side - 4),
                                  static_cast<native::dim>(side)),
                     true);
        _g.draw_rect(native::rect(static_cast<native::coord>(x),
                                  static_cast<native::coord>(y + 2),
                                  static_cast<native::dim>(side),
                                  static_cast<native::dim>(side - 4)),
                     true);
        _g.set_ink(colors.button_border)
            .draw_line(native::point(x + 2, y),
                       native::point(right - 2, y))
            .draw_line(native::point(right - 2, y),
                       native::point(right, y + 2))
            .draw_line(native::point(right, y + 2),
                       native::point(right, bottom - 2))
            .draw_line(native::point(right, bottom - 2),
                       native::point(right - 2, bottom))
            .draw_line(native::point(right - 2, bottom),
                       native::point(x + 2, bottom))
            .draw_line(native::point(x + 2, bottom),
                       native::point(x, bottom - 2))
            .draw_line(native::point(x, bottom - 2),
                       native::point(x, y + 2))
            .draw_line(native::point(x, y + 2),
                       native::point(x + 2, y));
        if (element_state.selected) {
            _g.set_ink(element_state.disabled
                           ? colors.button_disabled_text
                           : colors.button_text)
                .draw_rect(
                    native::rect(
                        static_cast<native::coord>(x + inset),
                        static_cast<native::coord>(y + inset),
                        static_cast<native::dim>(side - inset * 2),
                        static_cast<native::dim>(side - inset * 2)),
                    true);
        }
        draw_control_label(
            bounds, right + 6, text, element_state, colors);
        return *this;
    }

    native::theme &
    emulated_theme::draw_list(const native::rect &bounds,
                              const std::vector<std::string> &items,
                              int selected_index,
                              const state &element_state) {
        saved_state saved(_g);
        const palette colors = native_palette();
        _g.set_pen(1)
            .set_ink(colors.menu_popup_bg)
            .draw_rect(bounds, true);
        _g.set_ink(colors.menu_popup_border).draw_rect(bounds, false);
        if (bounds.d.w <= 2 || bounds.d.h <= 2)
            return *this;
        const native::rect content(
            bounds.p.x + 1,
            bounds.p.y + 1,
            static_cast<native::dim>(bounds.d.w - 2),
            static_cast<native::dim>(bounds.d.h - 2));
        _g.set_clip(_g.get_clip().intersect(content));
        const int item_height =
            std::max(1, defaults().list_item_height);
        for (std::size_t i = 0; i < items.size(); ++i) {
            const int y =
                content.p.y + static_cast<int>(i) * item_height;
            if (y >= content.y2())
                break;
            state item_state = element_state;
            item_state.selected = static_cast<int>(i) == selected_index;
            draw_list_item(
                native::rect(content.p.x,
                             static_cast<native::coord>(y),
                             content.d.w,
                             static_cast<native::dim>(std::min(
                                 item_height, content.y2() - y))),
                items[i],
                item_state);
        }
        return *this;
    }

    native::theme &emulated_theme::draw_text_edit_frame(
        const native::rect &bounds,
        const state &element_state) {
        saved_state saved(_g);
        const palette colors = native_palette();
        const native::rgba background = element_state.disabled
                                            ? colors.button_bg
                                            : colors.menu_popup_bg;
        _g.set_pen(1).set_ink(background).draw_rect(bounds, true);
        draw_bevel(bounds, true, colors);
        return *this;
    }

    int emulated_theme::text_y(const native::rect &bounds) const {
        return bounds.p.y +
               std::max(0,
                        (static_cast<int>(bounds.d.h) - text_height()) /
                            2);
    }

    emulated_theme::saved_state::saved_state(native::gpx &graphics)
        : _graphics(graphics)
        , _ink(graphics.get_ink())
        , _paper(graphics.get_paper())
        , _pen(graphics.get_pen())
        , _font(graphics.get_font())
        , _clip(graphics.get_clip()) {}

    emulated_theme::saved_state::~saved_state() {
        _graphics.set_ink(_ink)
            .set_paper(_paper)
            .set_pen(_pen)
            .set_font(_font);
        _graphics.set_clip(_clip);
    }

    void emulated_theme::draw_bevel(const native::rect &bounds,
                                    bool inset,
                                    const palette &colors) {
        if (!bounds.d.w || !bounds.d.h)
            return;
        const native::rgba top =
            inset ? colors.button_shadow : colors.button_highlight;
        const native::rgba bottom =
            inset ? colors.button_highlight : colors.button_shadow;
        _g.set_ink(top)
            .draw_line(bounds.p,
                       native::point(bounds.x2() - 1, bounds.p.y))
            .draw_line(bounds.p,
                       native::point(bounds.p.x, bounds.y2() - 1));
        _g.set_ink(bottom)
            .draw_line(native::point(bounds.p.x, bounds.y2() - 1),
                       native::point(bounds.x2() - 1, bounds.y2() - 1))
            .draw_line(native::point(bounds.x2() - 1, bounds.p.y),
                       native::point(bounds.x2() - 1, bounds.y2() - 1));
        _g.set_ink(colors.button_border).draw_rect(bounds, false);
    }

    void emulated_theme::draw_indicator_box(const native::rect &bounds,
                                            bool inset,
                                            const palette &colors) {
        _g.set_pen(1).set_ink(colors.button_bg).draw_rect(bounds, true);
        draw_bevel(bounds, inset, colors);
    }

    void emulated_theme::draw_control_label(const native::rect &bounds,
                                            int x,
                                            const std::string &text,
                                            const state &element_state,
                                            const palette &colors) {
        _g.set_font(native::font_t::stock(native::font_role::control));
        _g.set_clip(_g.get_clip().intersect(bounds));
        _g.set_ink(element_state.disabled ? colors.button_disabled_text
                                          : colors.button_text)
            .draw_text(text,
                       native::point(
                           static_cast<native::coord>(x),
                           static_cast<native::coord>(text_y(bounds))));
    }

    native::theme &
    emulated_theme::draw_menu_entry(const native::rect &bounds,
                                    const std::string &text,
                                    const state &element_state,
                                    bool title) {
        saved_state saved(_g);
        const palette colors = native_palette();
        const bool active = element_state.selected || element_state.hot;
        const native::rgba background =
            active
                ? colors.menu_hot_bg
                : (title ? colors.menu_bar_bg : colors.menu_popup_bg);
        const native::rgba foreground =
            element_state.disabled
                ? colors.menu_disabled_text
                : (active ? colors.menu_hot_text : colors.menu_text);

        _g.set_pen(1).set_ink(background).draw_rect(bounds, true);
        _g.set_font(native::font_t::stock(native::font_role::control));
        _g.set_clip(_g.get_clip().intersect(bounds));
        _g.set_ink(foreground)
            .draw_text(
                text,
                native::point(bounds.p.x + defaults().text_padding_x,
                              text_y(bounds)));
        return *this;
    }
} // namespace linux
