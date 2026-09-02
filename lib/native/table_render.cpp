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
            table_column_id fill_column = 0;
            int fill_width = 0;
            bool vertical = false;
            bool horizontal = false;
            rect body;
        };

        geometry table_geometry(table_view &control,
                                const theme::metrics &metrics) {
            geometry result;
            result.header = control.get_header_visible()
                                ? metrics.header_height
                                : 0;
            result.row = control.get_row_height()
                             ? *control.get_row_height()
                             : metrics.table_row_height;
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
            if (metrics.table_fill_last_column && !result.horizontal &&
                result.content_width < body_width) {
                for (auto column = control.get_columns().rbegin();
                     column != control.get_columns().rend(); ++column) {
                    if (!column->visible)
                        continue;
                    result.fill_column = column->id;
                    result.fill_width = column->width +
                                        body_width - result.content_width;
                    result.content_width = body_width;
                    break;
                }
            }
            return result;
        }

        int rendered_width(const geometry &layout,
                           const table_column &column) {
            return column.id == layout.fill_column
                       ? layout.fill_width
                       : column.width;
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

    } // namespace

    void draw_table_view(table_view &control, gpx &graphics) {
        auto saved = graphics.save_state();
        auto painter = theme::create(graphics);
        const theme::metrics metrics = painter->defaults();
        const geometry layout = table_geometry(control, metrics);
        const rect bounds(0, 0,
                          control.get_dimensions().w,
                          control.get_dimensions().h);
        graphics.set_clip(graphics.get_clip().intersect(bounds));
        theme::state control_state;
        control_state.focused = control.get_focused();
        control.draw_background(
            graphics, *painter, bounds, control_state);

        graphics.set_font(font_t::stock(font_role::control));
        int column_x = -control.get_horizontal_scroll_offset();
        if (layout.header > 0) {
            for (const auto &column : control.get_columns()) {
                if (!column.visible)
                    continue;
                const int width = rendered_width(layout, column);
                const rect cell_bounds(
                    static_cast<coord>(std::clamp(
                        column_x,
                        static_cast<int>(
                            std::numeric_limits<coord>::min()),
                        static_cast<int>(
                            std::numeric_limits<coord>::max()))),
                    0,
                    static_cast<dim>(width),
                    static_cast<dim>(layout.header));
                theme::state header_state = control_state;
                control.draw_header_background(
                    graphics, *painter, column, cell_bounds,
                    header_state);
                control.draw_header_content(
                    graphics, *painter, column, cell_bounds,
                    header_state);
                control.draw_header_border(
                    graphics, *painter, column, cell_bounds,
                    header_state);
                column_x += width;
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
                theme::state group_state = control_state;
                const auto group = model
                    ? group_by_id(*model, display.group_id)
                    : std::nullopt;
                if (group)
                    control.draw_group(
                        graphics, *painter, *group, row_bounds,
                        group_state);
                continue;
            }
            if (!model)
                continue;
            const table_row_id row_id =
                model->row_id(display.model_row);
            const bool is_selected =
                std::find(selected.begin(), selected.end(), row_id) !=
                selected.end();
            theme::state row_state = control_state;
            row_state.selected = is_selected;
            row_state.focused = is_selected && control.get_focused();
            control.draw_row_background(
                graphics, *painter, row_id, display.model_row,
                row_bounds, row_state);
            column_x = -control.get_horizontal_scroll_offset();
            for (const auto &column : control.get_columns()) {
                if (!column.visible)
                    continue;
                const int width = rendered_width(layout, column);
                const table_cell value =
                    model->cell(display.model_row, column.id);
                const rect cell_bounds(
                    static_cast<coord>(column_x),
                    static_cast<coord>(y),
                    static_cast<dim>(width),
                    static_cast<dim>(layout.row));
                control.draw_cell_background(
                    graphics, *painter, row_id, display.model_row,
                    column, value, cell_bounds, row_state);
                control.draw_cell_content(
                    graphics, *painter, row_id, display.model_row,
                    column, value, cell_bounds, row_state);
                control.draw_cell_border(
                    graphics, *painter, row_id, display.model_row,
                    column, value, cell_bounds, row_state);
                column_x += width;
            }
            control.draw_row_focus(
                graphics, *painter, row_id, display.model_row,
                row_bounds, row_state);
        }
        graphics.set_clip(old_clip);

        if (layout.vertical) {
            const rect track(
                static_cast<coord>(layout.body.x2()),
                static_cast<coord>(layout.header),
                static_cast<dim>(layout.scrollbar),
                layout.body.d.h);
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
            control.draw_scrollbar(
                graphics,
                *painter,
                scrollbar_orientation::vertical,
                track,
                rect(track.p.x,
                     static_cast<coord>(thumb_y),
                     track.d.w,
                     static_cast<dim>(thumb_height)),
                control_state);
        }
        if (layout.horizontal) {
            const rect track(
                0,
                static_cast<coord>(layout.body.y2()),
                layout.body.d.w,
                static_cast<dim>(layout.scrollbar));
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
            control.draw_scrollbar(
                graphics,
                *painter,
                scrollbar_orientation::horizontal,
                track,
                rect(static_cast<coord>(thumb_x),
                     track.p.y,
                     static_cast<dim>(thumb_width),
                     track.d.h),
                control_state);
        }
        // The viewport relief is intentionally last. Header cells may touch
        // its bounds and must not erase it. Native scrollbar reservations
        // remain outside the viewport as independently framed controls.
        const rect viewport_bounds(
            0,
            0,
            layout.body.d.w,
            static_cast<dim>(layout.header + layout.body.d.h));
        control.draw_border(
            graphics, *painter, viewport_bounds, control_state);
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
                const int width = rendered_width(layout, column);
                if (x >= 0 && x < width) {
                    control.on_native_sort_request(column.id);
                    return true;
                }
                x -= width;
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
