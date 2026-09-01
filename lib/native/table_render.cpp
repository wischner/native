//
// Implements the shared visible-row table painter using only semantic
// native theme parts and the portable graphics API.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "table_render.h"

#include <algorithm>
#include <limits>
#include <optional>

#include <native/font.h>
#include <native/graphics.h>
#include <native/table_view.h>
#include <native/theme.h>

namespace native::detail
{
    namespace
    {
        class offset_graphics final : public gpx
        {
        public:
            offset_graphics(gpx &target, point origin, size dimensions)
                : _target(target)
                , _origin(origin)
                , _clip(0, 0, dimensions.w, dimensions.h) {
                _ink = target.get_ink();
                _paper = target.get_paper();
                _thickness = target.get_pen();
                _font = &target.get_font();
            }

            gpx &set_clip(const rect &bounds) override {
                _clip = bounds;
                return *this;
            }

            rect get_clip() const override { return _clip; }

            gpx &clear(rgba color) override {
                apply();
                _target.set_ink(color).draw_rect(translated(_clip), true);
                return *this;
            }

            gpx &draw_line(point from, point to) override {
                apply();
                _target.draw_line(translated(from), translated(to));
                return *this;
            }

            gpx &draw_rect(rect bounds, bool filled) override {
                apply();
                _target.draw_rect(translated(bounds), filled);
                return *this;
            }

            gpx &draw_img(const img &source, point destination) override {
                apply();
                _target.draw_img(source, translated(destination));
                return *this;
            }

        protected:
            gpx &draw_native_text(const std::string &text,
                                  point position) override {
                apply();
                _target.draw_text(text, translated(position));
                return *this;
            }

        private:
            gpx &_target;
            point _origin;
            rect _clip;

            point translated(point value) const {
                return point(
                    static_cast<coord>(value.x + _origin.x),
                    static_cast<coord>(value.y + _origin.y));
            }

            rect translated(rect value) const {
                value.p = translated(value.p);
                return value;
            }

            void apply() {
                _target.set_ink(_ink)
                    .set_paper(_paper)
                    .set_pen(_thickness)
                    .set_font(get_font())
                    .set_clip(translated(_clip));
            }
        };

        struct geometry
        {
            int header = 0;
            int row = 20;
            int scrollbar = 16;
            int content_width = 0;
            bool vertical = false;
            bool horizontal = false;
            rect body;
        };

        bool has_line(table_grid_lines value,
                      table_grid_lines flag) {
            return (static_cast<std::uint8_t>(value) &
                    static_cast<std::uint8_t>(flag)) != 0;
        }

        rgba mixed(rgba first, rgba second, int second_weight) {
            const auto channel = [second_weight](std::uint8_t a,
                                                 std::uint8_t b) {
                return static_cast<std::uint8_t>(
                    (static_cast<int>(a) * (100 - second_weight) +
                     static_cast<int>(b) * second_weight) /
                    100);
            };
            return rgba(channel(first.r, second.r),
                        channel(first.g, second.g),
                        channel(first.b, second.b),
                        first.a);
        }

        geometry table_geometry(table_view &control,
                                const theme::metrics &metrics) {
            geometry result;
            result.header = control.get_header_visible()
                                ? metrics.header_height
                                : 0;
            result.row = control.get_row_height()
                             ? *control.get_row_height()
                             : metrics.list_item_height;
            result.row = std::max(1, result.row);
            result.scrollbar = std::max(1, metrics.scrollbar_extent);
            for (const auto &column : control.get_columns()) {
                if (column.visible)
                    result.content_width += column.width;
            }
            int body_width = control.get_dimensions().w;
            int body_height = std::max(
                0,
                static_cast<int>(control.get_dimensions().h) -
                    result.header);
            const bool needs_vertical =
                static_cast<std::uint64_t>(
                    control.get_display_row_count()) * result.row >
                static_cast<std::uint64_t>(body_height);
            result.vertical =
                control.get_vertical_scrollbar_policy() ==
                    scrollbar_policy::always ||
                (control.get_vertical_scrollbar_policy() ==
                     scrollbar_policy::automatic &&
                 needs_vertical);
            if (result.vertical)
                body_width = std::max(0, body_width - result.scrollbar);
            const bool needs_horizontal =
                result.content_width > body_width;
            result.horizontal =
                control.get_horizontal_scrollbar_policy() ==
                    scrollbar_policy::always ||
                (control.get_horizontal_scrollbar_policy() ==
                     scrollbar_policy::automatic &&
                 needs_horizontal);
            if (result.horizontal)
                body_height = std::max(0,
                                       body_height - result.scrollbar);
            result.body = rect(
                0,
                static_cast<coord>(result.header),
                static_cast<dim>(body_width),
                static_cast<dim>(body_height));
            return result;
        }

        std::optional<table_group> group_by_id(table_model &model,
                                                table_group_id id) {
            for (std::size_t index = 0;
                 index < model.group_count(); ++index) {
                table_group group = model.group(index);
                if (group.id == id)
                    return group;
            }
            return std::nullopt;
        }

        rect fitted(const img &image, rect box) {
            if (!box.d.w || !box.d.h)
                return rect(box.p, size());
            const double scale = std::min(
                {1.0,
                 static_cast<double>(box.d.w) / image.w(),
                 static_cast<double>(box.d.h) / image.h()});
            const int width = std::max(
                1, static_cast<int>(image.w() * scale));
            const int height = std::max(
                1, static_cast<int>(image.h() * scale));
            return rect(
                static_cast<coord>(box.p.x +
                    (static_cast<int>(box.d.w) - width) / 2),
                static_cast<coord>(box.p.y +
                    (static_cast<int>(box.d.h) - height) / 2),
                static_cast<dim>(width),
                static_cast<dim>(height));
        }

        int aligned_x(table_alignment alignment,
                      int cell_x,
                      int cell_width,
                      int text_width,
                      int padding) {
            if (alignment == table_alignment::center)
                return cell_x + (cell_width - text_width) / 2;
            if (alignment == table_alignment::end)
                return cell_x + cell_width - text_width - padding;
            return cell_x + padding;
        }
    } // namespace

    void draw_table_view(table_view &control, gpx &graphics) {
        auto saved = graphics.save_state();
        auto painter = theme::create(graphics);
        const theme::metrics metrics = painter->defaults();
        const theme::palette colors = painter->native_palette();
        const geometry layout = table_geometry(control, metrics);
        const rect bounds(0, 0,
                          control.get_dimensions().w,
                          control.get_dimensions().h);
        graphics.set_clip(graphics.get_clip().intersect(bounds));
        painter->draw_surface(bounds,
                              surface_kind::inset,
                              theme::state{});

        graphics.set_font(font_t::stock(font_role::control));
        int column_x = -control.get_horizontal_scroll_offset();
        if (layout.header > 0) {
            for (const auto &column : control.get_columns()) {
                if (!column.visible)
                    continue;
                const rect cell_bounds(
                    static_cast<coord>(std::clamp(
                        column_x,
                        static_cast<int>(
                            std::numeric_limits<coord>::min()),
                        static_cast<int>(
                            std::numeric_limits<coord>::max()))),
                    0,
                    column.width,
                    static_cast<dim>(layout.header));
                painter->draw_surface(cell_bounds,
                                      surface_kind::header,
                                      theme::state{});
                graphics.set_ink(colors.button_text).draw_text(
                    column.title,
                    rect(static_cast<coord>(cell_bounds.p.x +
                                             metrics.header_padding_x),
                         cell_bounds.p.y,
                         static_cast<dim>(std::max(
                             0,
                             static_cast<int>(cell_bounds.d.w) -
                                 metrics.header_padding_x * 2 - 10)),
                         cell_bounds.d.h),
                    text_layout{text_align::start,
                                text_valign::center,
                                text_overflow::ellipsis,
                                true});
                if (control.get_sort() &&
                    control.get_sort()->column == column.id) {
                    const int middle = layout.header / 2;
                    const int right = cell_bounds.x2() - 6;
                    const bool ascending =
                        control.get_sort()->direction ==
                        sort_direction::ascending;
                    const std::vector<point> arrow = ascending
                        ? std::vector<point>{
                              point(static_cast<coord>(right - 6),
                                    static_cast<coord>(middle + 2)),
                              point(static_cast<coord>(right),
                                    static_cast<coord>(middle + 2)),
                              point(static_cast<coord>(right - 3),
                                    static_cast<coord>(middle - 2))}
                        : std::vector<point>{
                              point(static_cast<coord>(right - 6),
                                    static_cast<coord>(middle - 2)),
                              point(static_cast<coord>(right),
                                    static_cast<coord>(middle - 2)),
                              point(static_cast<coord>(right - 3),
                                    static_cast<coord>(middle + 2))};
                    graphics.draw_polygon(arrow, true);
                }
                painter->draw_separator(
                    rect(static_cast<coord>(cell_bounds.x2() - 1),
                         0,
                         1,
                         cell_bounds.d.h),
                    separator_orientation::vertical);
                column_x += column.width;
            }
            painter->draw_separator(
                rect(0,
                     static_cast<coord>(layout.header - 1),
                     control.get_dimensions().w,
                     1),
                separator_orientation::horizontal);
        }

        const rect old_clip = graphics.get_clip();
        graphics.set_clip(old_clip.intersect(layout.body));
        const table_visible_range visible =
            control.get_visible_row_range();
        table_model *model = control.get_model();
        const auto selected = control.get_selected_rows();
        for (std::size_t index = 0;
             index < visible.count; ++index) {
            const std::size_t display_index = visible.first + index;
            const table_display_row display =
                control.get_display_row(display_index);
            const int y = layout.header +
                          static_cast<int>(index) * layout.row;
            const rect row_bounds(0,
                                  static_cast<coord>(y),
                                  layout.body.d.w,
                                  static_cast<dim>(layout.row));
            if (display.group) {
                theme::state group_state;
                painter->draw_surface(row_bounds,
                                      surface_kind::header,
                                      group_state);
                const auto group = model
                    ? group_by_id(*model, display.group_id)
                    : std::nullopt;
                int text_x = metrics.header_padding_x;
                if (group && group->collapsible) {
                    const int side = std::min(
                        metrics.disclosure_size,
                        std::max(1, layout.row - 4));
                    const rect disclosure(
                        static_cast<coord>(metrics.header_padding_x),
                        static_cast<coord>(y +
                            (layout.row - side) / 2),
                        static_cast<dim>(side),
                        static_cast<dim>(side));
                    painter->draw_disclosure(
                        disclosure,
                        control.get_group_expanded(group->id)
                            ? disclosure_state::expanded
                            : disclosure_state::collapsed,
                        group_state);
                    text_x = disclosure.x2() + metrics.header_gap;
                }
                graphics.set_ink(colors.button_text).draw_text(
                    group ? group->title : std::string(),
                    rect(static_cast<coord>(text_x),
                         static_cast<coord>(y),
                         static_cast<dim>(std::max(
                             0,
                             static_cast<int>(layout.body.d.w) - text_x)),
                         static_cast<dim>(layout.row)),
                    text_layout{text_align::start,
                                text_valign::center,
                                text_overflow::ellipsis,
                                true});
                continue;
            }
            if (!model)
                continue;
            const table_row_id row_id =
                model->row_id(display.model_row);
            const bool is_selected =
                std::find(selected.begin(), selected.end(), row_id) !=
                selected.end();
            if (control.get_alternating_rows() &&
                (display.model_row & 1U) != 0) {
                graphics.set_ink(mixed(colors.content_bg,
                                       colors.separator,
                                       8))
                    .draw_rect(row_bounds, true);
            } else {
                graphics.set_ink(colors.content_bg)
                    .draw_rect(row_bounds, true);
            }
            theme::state row_state;
            row_state.selected = is_selected;
            row_state.focused = is_selected && control.get_focused();
            painter->draw_selection(row_bounds,
                                    selection_shape::row,
                                    row_state);
            column_x = -control.get_horizontal_scroll_offset();
            for (const auto &column : control.get_columns()) {
                if (!column.visible)
                    continue;
                const table_cell value =
                    model->cell(display.model_row, column.id);
                int text_x = column_x + metrics.header_padding_x;
                int available = column.width -
                                metrics.header_padding_x * 2;
                if (value.image && column.allow_image) {
                    const size requested = control.get_icon_size()
                        .value_or(size(
                            static_cast<dim>(std::max(
                                1, layout.row - 6)),
                            static_cast<dim>(std::max(
                                1, layout.row - 6))));
                    const int side_width = std::min<int>(
                        requested.w, std::max(1, available));
                    const int side_height = std::min<int>(
                        requested.h, std::max(1, layout.row - 4));
                    const rect image_box(
                        static_cast<coord>(text_x),
                        static_cast<coord>(y +
                            (layout.row - side_height) / 2),
                        static_cast<dim>(side_width),
                        static_cast<dim>(side_height));
                    graphics.draw_img(*value.image,
                                      fitted(*value.image, image_box),
                                      image_filter::linear);
                    text_x = image_box.x2() + metrics.header_gap;
                    available -= side_width + metrics.header_gap;
                }
                const text_metrics measured =
                    graphics.measure_text(value.text);
                if (!value.image ||
                    column.alignment != table_alignment::start) {
                    text_x = aligned_x(column.alignment,
                                       column_x,
                                       column.width,
                                       measured.width,
                                       metrics.header_padding_x);
                }
                graphics.set_ink(is_selected
                                     ? colors.selection_text
                                     : colors.content_text)
                    .draw_text(
                        value.text,
                        rect(static_cast<coord>(text_x),
                             static_cast<coord>(y),
                             static_cast<dim>(std::max(0, available)),
                             static_cast<dim>(layout.row)),
                        text_layout{text_align::start,
                                    text_valign::center,
                                    text_overflow::ellipsis,
                                    true});
                if (has_line(control.get_grid_lines(),
                             table_grid_lines::vertical)) {
                    painter->draw_separator(
                        rect(static_cast<coord>(
                                 column_x + column.width - 1),
                             static_cast<coord>(y),
                             1,
                             static_cast<dim>(layout.row)),
                        separator_orientation::vertical);
                }
                column_x += column.width;
            }
            if (has_line(control.get_grid_lines(),
                         table_grid_lines::horizontal)) {
                painter->draw_separator(
                    rect(0,
                         static_cast<coord>(y + layout.row - 1),
                         layout.body.d.w,
                         1),
                    separator_orientation::horizontal);
            }
            painter->draw_focus(row_bounds, row_state);
        }
        graphics.set_clip(old_clip);

        if (layout.vertical) {
            const rect track(
                static_cast<coord>(layout.body.x2()),
                static_cast<coord>(layout.header),
                static_cast<dim>(layout.scrollbar),
                layout.body.d.h);
            painter->draw_scrollbar_part(
                track,
                scrollbar_orientation::vertical,
                scrollbar_part::track,
                theme::state{});
            const std::size_t total = std::max<std::size_t>(
                1, control.get_display_row_count());
            const std::size_t page = std::max<std::size_t>(
                1, control.get_visible_row_range().count);
            const int thumb_height = std::min(
                static_cast<int>(track.d.h),
                std::max(metrics.scrollbar_min_thumb,
                         static_cast<int>(track.d.h * page / total)));
            const std::size_t maximum = total > page ? total - page : 0;
            const int thumb_y = maximum == 0
                ? track.p.y
                : track.p.y + static_cast<int>(
                      control.get_vertical_scroll_row() *
                      (track.d.h - thumb_height) / maximum);
            painter->draw_scrollbar_part(
                rect(track.p.x,
                     static_cast<coord>(thumb_y),
                     track.d.w,
                     static_cast<dim>(thumb_height)),
                scrollbar_orientation::vertical,
                scrollbar_part::thumb,
                theme::state{});
        }
        if (layout.horizontal) {
            const rect track(
                0,
                static_cast<coord>(layout.body.y2()),
                layout.body.d.w,
                static_cast<dim>(layout.scrollbar));
            painter->draw_scrollbar_part(
                track,
                scrollbar_orientation::horizontal,
                scrollbar_part::track,
                theme::state{});
            const int total = std::max(1, layout.content_width);
            const int page = std::max(1,
                                      static_cast<int>(layout.body.d.w));
            const int thumb_width = std::min(
                static_cast<int>(track.d.w),
                std::max(metrics.scrollbar_min_thumb,
                         static_cast<int>(track.d.w) * page / total));
            const int maximum = std::max(0, total - page);
            const int thumb_x = maximum == 0
                ? 0
                : control.get_horizontal_scroll_offset() *
                      (track.d.w - thumb_width) / maximum;
            painter->draw_scrollbar_part(
                rect(static_cast<coord>(thumb_x),
                     track.p.y,
                     static_cast<dim>(thumb_width),
                     track.d.h),
                scrollbar_orientation::horizontal,
                scrollbar_part::thumb,
                theme::state{});
        }
    }

    void draw_table_view_at(table_view &control,
                            gpx &graphics,
                            point origin) {
        auto saved = graphics.save_state();
        offset_graphics translated(
            graphics, origin, control.get_dimensions());
        draw_table_view(control, translated);
    }

    bool handle_table_click(table_view &control, point position) {
        auto painter = theme::create(control.get_gpx());
        const theme::metrics metrics = painter->defaults();
        const geometry layout = table_geometry(control, metrics);
        if (position.y < layout.header) {
            int x = position.x +
                    control.get_horizontal_scroll_offset();
            for (const auto &column : control.get_columns()) {
                if (!column.visible)
                    continue;
                if (x >= 0 && x < column.width) {
                    control.on_native_sort_request(column.id);
                    return true;
                }
                x -= column.width;
            }
            return false;
        }
        if (!layout.body.contains(position))
            return false;
        const std::size_t offset = static_cast<std::size_t>(
            (position.y - layout.header) / layout.row);
        const std::size_t display =
            control.get_vertical_scroll_row() + offset;
        if (display >= control.get_display_row_count())
            return false;
        const table_display_row row = control.get_display_row(display);
        if (row.group) {
            table_model *model = control.get_model();
            const auto group = model
                ? group_by_id(*model, row.group_id)
                : std::nullopt;
            if (group && group->collapsible) {
                control.on_native_group_expand(
                    group->id,
                    !control.get_group_expanded(group->id));
                return true;
            }
            return false;
        }
        table_model *model = control.get_model();
        if (!model)
            return false;
        control.on_native_selection({model->row_id(row.model_row)});
        return true;
    }
} // namespace native::detail
