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
#include <limits>

#include <native/graphics.h>
#include <native/ruler.h>

namespace native
{
    namespace
    {
        rgba opaque(rgba color, rgba fallback) {
            if (color.a == 0)
                color = fallback;
            color.a = 255;
            return color;
        }

        rgba alternate_content(const theme::palette &colors) {
            if (colors.content_alt_bg.a != 0)
                return opaque(colors.content_alt_bg, colors.content_bg);

            return rgba(
                static_cast<std::uint8_t>(
                    (static_cast<unsigned int>(colors.content_bg.r) *
                         247U +
                     static_cast<unsigned int>(colors.separator.r) * 8U) /
                    255U),
                static_cast<std::uint8_t>(
                    (static_cast<unsigned int>(colors.content_bg.g) *
                         247U +
                     static_cast<unsigned int>(colors.separator.g) * 8U) /
                    255U),
                static_cast<std::uint8_t>(
                    (static_cast<unsigned int>(colors.content_bg.b) *
                         247U +
                     static_cast<unsigned int>(colors.separator.b) * 8U) /
                    255U),
                255);
        }

        dim bounded_dimension(int value) {
            return static_cast<dim>(std::max(
                0,
                std::min(
                    value,
                    static_cast<int>(std::numeric_limits<dim>::max()))));
        }
    } // namespace

    theme::theme(gpx &painter)
        : _g(painter) {}

    theme::~theme() = default;

    int theme::get_button_height() const {
        return defaults().button_height;
    }

    int theme::get_menu_bar_height() const {
        return defaults().menu_bar_height;
    }

    int theme::get_menu_item_height() const {
        return defaults().menu_item_height;
    }

    int theme::get_popup_width() const {
        return defaults().popup_width;
    }

    int theme::get_text_padding_x() const {
        return defaults().text_padding_x;
    }

    int theme::get_text_edit_height() const {
        return defaults().text_edit_height;
    }

    int theme::get_check_height() const {
        return defaults().check_height;
    }

    int theme::get_radio_height() const {
        return defaults().radio_height;
    }

    int theme::get_list_item_height() const {
        return defaults().list_item_height;
    }

    int theme::get_table_row_height() const {
        return defaults().table_row_height;
    }

    int theme::get_table_outer_border_extent() const {
        return defaults().table_outer_border_extent;
    }

    int theme::get_focus_inset() const {
        return defaults().focus_inset;
    }

    int theme::get_disclosure_size() const {
        return defaults().disclosure_size;
    }

    int theme::get_sort_indicator_size() const {
        return defaults().sort_indicator_size;
    }

    int theme::get_caption_button_size() const {
        return defaults().caption_button_size;
    }

    bool theme::get_tree_lines_visible() const {
        return defaults().tree_lines_visible;
    }

    int theme::get_tree_row_height() const {
        return defaults().tree_row_height;
    }

    int theme::get_tree_horizontal_padding() const {
        return defaults().tree_horizontal_padding;
    }

    int theme::get_tree_indent_width() const {
        return defaults().tree_indent_width;
    }

    int theme::get_tree_item_gap() const {
        return defaults().tree_item_gap;
    }

    int theme::get_tree_icon_vertical_padding() const {
        return defaults().tree_icon_vertical_padding;
    }

    int theme::get_header_height() const {
        return defaults().header_height;
    }

    int theme::get_header_padding_x() const {
        return defaults().header_padding_x;
    }

    int theme::get_header_gap() const {
        return defaults().header_gap;
    }

    int theme::get_tab_height() const {
        return defaults().tab_height;
    }

    int theme::get_icon_view_padding_x() const {
        return defaults().icon_view_padding_x;
    }

    int theme::get_icon_view_padding_y() const {
        return defaults().icon_view_padding_y;
    }

    int theme::get_icon_view_item_gap_x() const {
        return defaults().icon_view_item_gap_x;
    }

    int theme::get_icon_view_item_gap_y() const {
        return defaults().icon_view_item_gap_y;
    }

    int theme::get_icon_view_label_gap() const {
        return defaults().icon_view_label_gap;
    }

    int theme::get_icon_view_min_item_width() const {
        return defaults().icon_view_min_item_width;
    }

    int theme::get_separator_extent() const {
        return defaults().separator_extent;
    }

    int theme::get_scrollbar_extent() const {
        return defaults().scrollbar_extent;
    }

    int theme::get_scrollbar_min_thumb() const {
        return defaults().scrollbar_min_thumb;
    }

    int theme::get_status_bar_height() const {
        return defaults().status_bar_height;
    }

    int theme::get_ruler_extent() const {
        return defaults().ruler_extent;
    }

    bool theme::get_table_fill_last_column() const {
        return defaults().table_fill_last_column;
    }

    size theme::get_separator_size(
        separator_orientation orientation,
        int length) const {
        const dim extent = bounded_dimension(get_separator_extent());
        const dim main = bounded_dimension(length);
        return orientation == separator_orientation::horizontal
                   ? size(main, extent)
                   : size(extent, main);
    }

    size theme::get_scrollbar_size(
        scrollbar_orientation orientation,
        int length) const {
        const dim extent = bounded_dimension(get_scrollbar_extent());
        const dim main = bounded_dimension(length);
        return orientation == scrollbar_orientation::horizontal
                   ? size(main, extent)
                   : size(extent, main);
    }

    size theme::get_status_bar_size(int width) const {
        return size(bounded_dimension(width),
                    bounded_dimension(get_status_bar_height()));
    }

    size theme::get_ruler_size(
        ruler_orientation orientation,
        int length) const {
        const dim extent = bounded_dimension(get_ruler_extent());
        const dim main = bounded_dimension(length);
        return orientation == ruler_orientation::horizontal
                   ? size(main, extent)
                   : size(extent, main);
    }

    rgba theme::get_button_background_color() const {
        const palette colors = native_palette();
        return opaque(colors.button_bg, colors.content_bg);
    }

    rgba theme::get_button_border_color() const {
        const palette colors = native_palette();
        return opaque(colors.button_border, colors.button_text);
    }

    rgba theme::get_button_highlight_color() const {
        const palette colors = native_palette();
        return opaque(colors.button_highlight, colors.button_bg);
    }

    rgba theme::get_button_shadow_color() const {
        const palette colors = native_palette();
        return opaque(colors.button_shadow, colors.button_border);
    }

    rgba theme::get_button_foreground_color() const {
        const palette colors = native_palette();
        return opaque(colors.button_text, colors.content_text);
    }

    rgba theme::get_button_disabled_foreground_color() const {
        const palette colors = native_palette();
        return opaque(colors.button_disabled_text, colors.button_text);
    }

    rgba theme::get_button_hot_background_color() const {
        const palette colors = native_palette();
        return opaque(colors.button_hot_bg, colors.button_bg);
    }

    rgba theme::get_button_hot_foreground_color() const {
        const palette colors = native_palette();
        return opaque(colors.button_hot_text, colors.button_text);
    }

    rgba theme::get_button_pressed_background_color() const {
        const palette colors = native_palette();
        return opaque(colors.button_pressed_bg, colors.button_bg);
    }

    rgba theme::get_button_pressed_foreground_color() const {
        const palette colors = native_palette();
        return opaque(colors.button_pressed_text, colors.button_text);
    }

    rgba theme::get_menu_bar_background_color() const {
        const palette colors = native_palette();
        return opaque(colors.menu_bar_bg, colors.button_bg);
    }

    rgba theme::get_menu_bar_top_color() const {
        const palette colors = native_palette();
        return opaque(colors.menu_bar_line_top, colors.menu_bar_bg);
    }

    rgba theme::get_menu_bar_bottom_color() const {
        const palette colors = native_palette();
        return opaque(colors.menu_bar_line_bottom, colors.menu_bar_bg);
    }

    rgba theme::get_menu_foreground_color() const {
        const palette colors = native_palette();
        return opaque(colors.menu_text, colors.button_text);
    }

    rgba theme::get_menu_disabled_foreground_color() const {
        const palette colors = native_palette();
        return opaque(colors.menu_disabled_text, colors.menu_text);
    }

    rgba theme::get_menu_hot_background_color() const {
        const palette colors = native_palette();
        return opaque(colors.menu_hot_bg, colors.menu_bar_bg);
    }

    rgba theme::get_menu_hot_foreground_color() const {
        const palette colors = native_palette();
        return opaque(colors.menu_hot_text, colors.menu_text);
    }

    rgba theme::get_menu_popup_background_color() const {
        const palette colors = native_palette();
        return opaque(colors.menu_popup_bg, colors.menu_bar_bg);
    }

    rgba theme::get_menu_popup_border_color() const {
        const palette colors = native_palette();
        return opaque(colors.menu_popup_border, colors.button_border);
    }

    rgba theme::get_content_background_color() const {
        const palette colors = native_palette();
        return opaque(colors.content_bg, colors.button_bg);
    }

    rgba theme::get_content_alternate_background_color() const {
        return alternate_content(native_palette());
    }

    rgba theme::get_content_foreground_color() const {
        const palette colors = native_palette();
        return opaque(colors.content_text, colors.button_text);
    }

    rgba theme::get_selection_background_color() const {
        const palette colors = native_palette();
        return opaque(colors.selection_bg, colors.button_pressed_bg);
    }

    rgba theme::get_selection_foreground_color() const {
        const palette colors = native_palette();
        return opaque(colors.selection_text, colors.content_text);
    }

    rgba theme::get_inactive_selection_background_color() const {
        const palette colors = native_palette();
        return opaque(colors.selection_inactive_bg,
                      colors.selection_bg);
    }

    rgba theme::get_inactive_selection_foreground_color() const {
        const palette colors = native_palette();
        return opaque(colors.selection_inactive_text,
                      colors.selection_text);
    }

    rgba theme::get_separator_color() const {
        const palette colors = native_palette();
        return opaque(colors.separator, colors.button_shadow);
    }

    rgba theme::get_focus_color() const {
        const palette colors = native_palette();
        return opaque(colors.focus, colors.button_text);
    }

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

    theme &theme::draw_sort_indicator(
        const rect &bounds,
        sort_indicator_state direction,
        const state &element_state) {
        auto saved = _g.save_state();
        return draw_sort_indicator_fallback(
            bounds, direction, element_state);
    }

    theme &theme::draw_caption_button(
        const rect &bounds,
        caption_button_kind kind,
        const state &element_state) {
        auto saved = _g.save_state();
        const palette colors = native_palette();
        draw_button(bounds, std::string(), element_state);
        const int inset = std::max(
            2, std::min<int>(bounds.d.w, bounds.d.h) / 4);
        _g.set_pen(element_state.pressed ? 2 : 1)
            .set_ink(element_state.disabled ? colors.button_disabled_text
                                            : colors.button_text);
        if (kind == caption_button_kind::close) {
            _g.draw_line(
                  point(bounds.p.x + inset, bounds.p.y + inset),
                  point(bounds.x2() - inset - 1,
                        bounds.y2() - inset - 1))
                .draw_line(
                  point(bounds.x2() - inset - 1, bounds.p.y + inset),
                  point(bounds.p.x + inset,
                        bounds.y2() - inset - 1));
            return *this;
        }
        const int center_x = bounds.p.x + bounds.d.w / 2;
        const int top = bounds.p.y + inset;
        const int half = std::max(2, inset);
        _g.draw_line(point(center_x - half, top),
                     point(center_x + half, top))
            .draw_line(point(center_x - half, top),
                       point(center_x, top + half))
            .draw_line(point(center_x + half, top),
                       point(center_x, top + half))
            .draw_line(
                point(center_x, top + half),
                point(kind == caption_button_kind::pin
                          ? center_x
                          : center_x + half,
                      bounds.y2() - 2));
        return *this;
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
        case surface_kind::table_header:
            fill = element_state.pressed
                       ? colors.button_pressed_bg
                       : (element_state.hot ? colors.button_hot_bg
                                            : colors.button_bg);
            border = colors.separator;
            break;
        case surface_kind::status:
            fill = colors.button_bg;
            break;
        case surface_kind::status_part:
            fill = colors.button_bg;
            border = colors.button_shadow;
            break;
        }
        _g.set_pen(1).set_ink(fill).draw_rect(bounds, true);
        if (kind == surface_kind::status_part &&
            bounds.d.w && bounds.d.h) {
            _g.set_ink(colors.button_highlight)
                .draw_line(bounds.p,
                           point(bounds.x2() - 1, bounds.p.y))
                .draw_line(bounds.p,
                           point(bounds.p.x, bounds.y2() - 1));
            _g.set_ink(border)
                .draw_line(point(bounds.p.x, bounds.y2() - 1),
                           point(bounds.x2() - 1, bounds.y2() - 1))
                .draw_line(point(bounds.x2() - 1, bounds.p.y),
                           point(bounds.x2() - 1, bounds.y2() - 1));
        } else if (kind == surface_kind::inset ||
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
                         : (element_state.selected
                                ? colors.selection_text
                                : colors.button_text))
            .draw_polygon(triangle, true);
        return *this;
    }

    theme &theme::draw_sort_indicator_fallback(
        const rect &bounds,
        sort_indicator_state direction,
        const state &element_state) {
        if (!bounds.d.w || !bounds.d.h)
            return *this;
        const palette colors = native_palette();
        const int side = std::max(
            3, std::min<int>(bounds.d.w, bounds.d.h));
        const int left = bounds.p.x +
                         (static_cast<int>(bounds.d.w) - side) / 2;
        const int top = bounds.p.y +
                        (static_cast<int>(bounds.d.h) - side) / 2;
        const int right = left + side - 1;
        const int bottom = top + side - 1;
        const int middle = left + side / 2;
        const std::vector<point> arrow =
            direction == sort_indicator_state::ascending
                ? std::vector<point>{
                      point(static_cast<coord>(left),
                            static_cast<coord>(bottom)),
                      point(static_cast<coord>(right),
                            static_cast<coord>(bottom)),
                      point(static_cast<coord>(middle),
                            static_cast<coord>(top))}
                : std::vector<point>{
                      point(static_cast<coord>(left),
                            static_cast<coord>(top)),
                      point(static_cast<coord>(right),
                            static_cast<coord>(top)),
                      point(static_cast<coord>(middle),
                            static_cast<coord>(bottom))};
        _g.set_pen(1)
            .set_ink(element_state.disabled
                         ? colors.button_disabled_text
                         : colors.button_text)
            .draw_polygon(arrow, true);
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
        scrollbar_orientation orientation,
        scrollbar_part part,
        const state &element_state) {
        const palette colors = native_palette();
        const bool control = part != scrollbar_part::track;
        const rgba fill = control
                              ? (element_state.pressed
                                     ? colors.button_pressed_bg
                                     : colors.button_bg)
                              : colors.content_bg;
        _g.set_pen(1).set_ink(fill).draw_rect(bounds, true);
        if (!control || !bounds.d.w || !bounds.d.h)
            return *this;

        _g.set_ink(colors.button_border).draw_rect(bounds, false);
        _g.set_ink(colors.button_highlight)
            .draw_line(bounds.p,
                       point(bounds.x2() - 1, bounds.p.y))
            .draw_line(bounds.p,
                       point(bounds.p.x, bounds.y2() - 1));
        _g.set_ink(colors.button_shadow)
            .draw_line(point(bounds.p.x, bounds.y2() - 1),
                       point(bounds.x2() - 1, bounds.y2() - 1))
            .draw_line(point(bounds.x2() - 1, bounds.p.y),
                       point(bounds.x2() - 1, bounds.y2() - 1));

        if (part == scrollbar_part::thumb) {
            _g.set_ink(colors.button_shadow);
            if (orientation == scrollbar_orientation::vertical) {
                const int center = bounds.p.y + bounds.d.h / 2;
                for (int offset = -2; offset <= 2; offset += 2) {
                    _g.draw_line(
                        point(bounds.p.x + 4,
                              static_cast<coord>(center + offset)),
                        point(bounds.x2() - 5,
                              static_cast<coord>(center + offset)));
                }
            } else {
                const int center = bounds.p.x + bounds.d.w / 2;
                for (int offset = -2; offset <= 2; offset += 2) {
                    _g.draw_line(
                        point(static_cast<coord>(center + offset),
                              bounds.p.y + 4),
                        point(static_cast<coord>(center + offset),
                              bounds.y2() - 5));
                }
            }
            return *this;
        }

        const int inset = std::max(
            3, std::min<int>(bounds.d.w, bounds.d.h) / 3);
        const int left = bounds.p.x + inset;
        const int right = bounds.x2() - inset - 1;
        const int top = bounds.p.y + inset;
        const int bottom = bounds.y2() - inset - 1;
        const int center_x = (left + right) / 2;
        const int center_y = (top + bottom) / 2;
        std::vector<point> arrow;
        if (orientation == scrollbar_orientation::vertical) {
            arrow = part == scrollbar_part::decrement
                ? std::vector<point>{point(left, bottom),
                                     point(right, bottom),
                                     point(center_x, top)}
                : std::vector<point>{point(left, top),
                                     point(right, top),
                                     point(center_x, bottom)};
        } else {
            arrow = part == scrollbar_part::decrement
                ? std::vector<point>{point(right, top),
                                     point(right, bottom),
                                     point(left, center_y)}
                : std::vector<point>{point(left, top),
                                     point(left, bottom),
                                     point(right, center_y)};
        }
        _g.set_ink(element_state.disabled
                       ? colors.button_disabled_text
                       : colors.button_text)
            .draw_polygon(arrow, true);
        return *this;
    }
} // namespace native
