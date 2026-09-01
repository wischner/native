//
// Implements backend-neutral theme lifetime and default-state
// overloads. Appearance and native drawing remain in the selected
// backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/theme.h>

#include <algorithm>

#include <native/graphics.h>

namespace native
{
    theme::theme(gpx &painter)
        : _g(painter) {}

    theme::~theme() = default;

    theme &theme::draw_button(const rect &bounds,
                              const std::string &text) {
        return draw_button(bounds, text, state{});
    }

    theme &theme::draw_menu_title(const rect &bounds,
                                  const std::string &text) {
        return draw_menu_title(bounds, text, state{});
    }

    theme &theme::draw_menu_item(const rect &bounds,
                                 const std::string &text) {
        return draw_menu_item(bounds, text, state{});
    }

    theme &theme::draw_list_item(const rect &bounds,
                                 const std::string &text) {
        return draw_list_item(bounds, text, state{});
    }

    theme &theme::draw_check(const rect &bounds,
                             const std::string &text) {
        return draw_check(bounds, text, state{});
    }

    theme &theme::draw_radio(const rect &bounds,
                             const std::string &text) {
        return draw_radio(bounds, text, state{});
    }

    theme &theme::draw_list(const rect &bounds,
                            const std::vector<std::string> &items,
                            int selected_index) {
        return draw_list(bounds, items, selected_index, state{});
    }

    theme &theme::draw_text_edit_frame(const rect &bounds) {
        return draw_text_edit_frame(bounds, state{});
    }

    theme &theme::draw_surface(
        const rect &bounds,
        surface_kind kind,
        const state &element_state) {
        auto saved = _g.save_state();
        return draw_surface_fallback(bounds, kind, element_state);
    }

    theme &theme::draw_selection(
        const rect &bounds,
        selection_shape shape,
        const state &element_state) {
        auto saved = _g.save_state();
        return draw_selection_fallback(bounds, shape, element_state);
    }

    theme &theme::draw_focus(
        const rect &bounds,
        const state &element_state) {
        auto saved = _g.save_state();
        return draw_focus_fallback(bounds, element_state);
    }

    theme &theme::draw_disclosure(
        const rect &bounds,
        disclosure_state disclosure,
        const state &element_state) {
        auto saved = _g.save_state();
        return draw_disclosure_fallback(
            bounds, disclosure, element_state);
    }

    theme &theme::draw_separator(
        const rect &bounds,
        separator_orientation orientation) {
        auto saved = _g.save_state();
        return draw_separator_fallback(bounds, orientation);
    }

    theme &theme::draw_scrollbar_part(
        const rect &bounds,
        scrollbar_orientation orientation,
        scrollbar_part part,
        const state &element_state) {
        auto saved = _g.save_state();
        return draw_scrollbar_part_fallback(
            bounds, orientation, part, element_state);
    }

    theme &theme::draw_surface_fallback(
        const rect &bounds,
        surface_kind kind,
        const state &element_state) {
        const palette colors = native_palette();
        rgba fill = colors.content_bg;
        rgba border = colors.separator;
        switch (kind) {
        case surface_kind::panel:
            fill = colors.button_bg;
            break;
        case surface_kind::content:
            fill = colors.content_bg;
            break;
        case surface_kind::inset:
            fill = colors.content_bg;
            border = colors.button_shadow;
            break;
        case surface_kind::popup:
            fill = colors.menu_popup_bg;
            border = colors.menu_popup_border;
            break;
        case surface_kind::header:
            fill = element_state.pressed
                       ? colors.button_pressed_bg
                       : (element_state.hot ? colors.button_hot_bg
                                            : colors.button_bg);
            border = colors.separator;
            break;
        }
        _g.set_pen(1).set_ink(fill).draw_rect(bounds, true);
        if (kind == surface_kind::inset ||
            kind == surface_kind::popup) {
            _g.set_ink(border).draw_rect(bounds, false);
        }
        return *this;
    }

    theme &theme::draw_selection_fallback(
        const rect &bounds,
        selection_shape,
        const state &element_state) {
        if (!element_state.selected && !element_state.hot)
            return *this;
        const palette colors = native_palette();
        const rgba fill = element_state.selected
                              ? (element_state.active
                                     ? colors.selection_bg
                                     : colors.selection_inactive_bg)
                              : colors.button_hot_bg;
        _g.set_pen(1).set_ink(fill).draw_rect(bounds, true);
        return *this;
    }

    theme &theme::draw_focus_fallback(
        const rect &bounds,
        const state &element_state) {
        if (!element_state.focused || bounds.d.w == 0 ||
            bounds.d.h == 0) {
            return *this;
        }
        _g.set_pen(1)
            .set_ink(native_palette().focus)
            .draw_rect(bounds, false);
        return *this;
    }

    theme &theme::draw_disclosure_fallback(
        const rect &bounds,
        disclosure_state disclosure,
        const state &element_state) {
        const palette colors = native_palette();
        const int left = bounds.p.x;
        const int top = bounds.p.y;
        const int right = bounds.x2() - 1;
        const int bottom = bounds.y2() - 1;
        std::vector<point> triangle;
        if (disclosure == disclosure_state::expanded) {
            triangle = {point(left, top), point(right, top),
                        point((left + right) / 2, bottom)};
        } else {
            triangle = {point(left, top), point(right, (top + bottom) / 2),
                        point(left, bottom)};
        }
        _g.set_pen(1)
            .set_ink(element_state.disabled
                         ? colors.button_disabled_text
                         : colors.button_text)
            .draw_polygon(triangle, true);
        return *this;
    }

    theme &theme::draw_separator_fallback(
        const rect &bounds,
        separator_orientation orientation) {
        if (!bounds.d.w || !bounds.d.h)
            return *this;
        _g.set_pen(1).set_ink(native_palette().separator);
        if (orientation == separator_orientation::horizontal) {
            _g.draw_line(
                point(bounds.p.x,
                      static_cast<coord>(bounds.p.y + bounds.d.h / 2)),
                point(static_cast<coord>(bounds.x2() - 1),
                      static_cast<coord>(bounds.p.y + bounds.d.h / 2)));
        } else {
            _g.draw_line(
                point(static_cast<coord>(bounds.p.x + bounds.d.w / 2),
                      bounds.p.y),
                point(static_cast<coord>(bounds.p.x + bounds.d.w / 2),
                      static_cast<coord>(bounds.y2() - 1)));
        }
        return *this;
    }

    theme &theme::draw_scrollbar_part_fallback(
        const rect &bounds,
        scrollbar_orientation,
        scrollbar_part part,
        const state &element_state) {
        const palette colors = native_palette();
        const bool thumb = part == scrollbar_part::thumb;
        const rgba fill = thumb
                              ? (element_state.pressed
                                     ? colors.button_pressed_bg
                                     : colors.button_bg)
                              : colors.content_bg;
        _g.set_pen(1).set_ink(fill).draw_rect(bounds, true);
        if (thumb)
            _g.set_ink(colors.button_border).draw_rect(bounds, false);
        return *this;
    }
} // namespace native
