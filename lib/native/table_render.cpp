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

#include "classic_scrollbar.h"

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
            const auto needs_vertical = [&](int height) {
                return
                static_cast<std::uint64_t>(
                    control.get_display_row_count()) * result.row >
                    static_cast<std::uint64_t>(std::max(0, height));
            };
            const auto wants_vertical = [&](int height) {
                return control.get_vertical_scrollbar_policy() ==
                           scrollbar_policy::always ||
                       (control.get_vertical_scrollbar_policy() ==
                            scrollbar_policy::automatic &&
                        needs_vertical(height));
            };
            const auto wants_horizontal = [&](int width) {
                return control.get_horizontal_scrollbar_policy() ==
                           scrollbar_policy::always ||
                       (control.get_horizontal_scrollbar_policy() ==
                            scrollbar_policy::automatic &&
                        result.content_width > std::max(0, width));
            };
            result.vertical =
                wants_vertical(body_height);
            if (result.vertical)
                body_width = std::max(0, body_width - result.scrollbar);
            result.horizontal = wants_horizontal(body_width);
            if (result.horizontal)
                body_height = std::max(0,
                                       body_height - result.scrollbar);
            if (!result.vertical && wants_vertical(body_height)) {
                result.vertical = true;
                body_width = std::max(0, body_width - result.scrollbar);
                result.horizontal = wants_horizontal(body_width);
            }
            result.body = rect(
                0,
                static_cast<coord>(result.header),
                static_cast<dim>(body_width),
                static_cast<dim>(body_height));
            if (control.get_fill_last_column() && !result.horizontal &&
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

        classic_scrollbar_geometry vertical_scrollbar(
            const geometry &layout,
            table_view &control,
            const theme::metrics &metrics) {
            const rect bounds(
                static_cast<coord>(layout.body.x2()),
                static_cast<coord>(layout.header),
                static_cast<dim>(layout.scrollbar),
                layout.body.d.h);
            const std::size_t total = std::max<std::size_t>(
                1, control.get_display_row_count());
            const std::size_t page = std::max<std::size_t>(
                1, control.get_visible_row_range().count);
            return make_classic_scrollbar(
                bounds,
                scrollbar_orientation::vertical,
                total,
                page,
                control.get_vertical_scroll_row(),
                metrics.scrollbar_min_thumb);
        }

        classic_scrollbar_geometry horizontal_scrollbar(
            const geometry &layout,
            table_view &control,
            const theme::metrics &metrics) {
            const rect bounds(
                0,
                static_cast<coord>(layout.body.y2()),
                layout.body.d.w,
                static_cast<dim>(layout.scrollbar));
            const int total = std::max(1, layout.content_width);
            const int page = std::max(
                1, static_cast<int>(layout.body.d.w));
            return make_classic_scrollbar(
                bounds,
                scrollbar_orientation::horizontal,
                static_cast<std::uint64_t>(total),
                static_cast<std::uint64_t>(page),
                static_cast<std::uint64_t>(
                    control.get_horizontal_scroll_offset()),
                metrics.scrollbar_min_thumb);
        }

        int rendered_width(const geometry &layout,
                           const table_column &column) {
            return column.id == layout.fill_column
                       ? layout.fill_width
                       : column.width;
        }

        int rendered_header_width(const geometry &layout,
                                  const table_column &column) {
            const int width = rendered_width(layout, column);
            return column.id == layout.fill_column && layout.vertical
                       ? width + layout.scrollbar
                       : width;
        }

        rect rendered_row_bounds(const geometry &layout,
                                 std::size_t index,
                                 std::size_t count) {
            const std::uint64_t nominal_height =
                static_cast<std::uint64_t>(count) *
                static_cast<std::uint64_t>(layout.row);
            if (count == 0 || nominal_height <= layout.body.d.h) {
                return rect(
                    layout.body.p.x,
                    static_cast<coord>(
                        layout.body.p.y +
                        static_cast<int>(index) * layout.row),
                    layout.body.d.w,
                    static_cast<dim>(layout.row));
            }
            const int top = layout.body.p.y + static_cast<int>(
                static_cast<std::uint64_t>(layout.body.d.h) * index /
                count);
            const int bottom = layout.body.p.y + static_cast<int>(
                static_cast<std::uint64_t>(layout.body.d.h) * (index + 1) /
                count);
            return rect(
                layout.body.p.x,
                static_cast<coord>(top),
                layout.body.d.w,
                static_cast<dim>(std::max(0, bottom - top)));
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
                const int width = rendered_header_width(layout, column);
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
            const rect row_bounds = rendered_row_bounds(
                layout, index, visible.count);
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
                    row_bounds.p.y,
                    static_cast<dim>(width),
                    row_bounds.d.h);
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
            const classic_scrollbar_geometry scrollbar =
                vertical_scrollbar(layout, control, metrics);
            control.draw_scrollbar(
                graphics,
                *painter,
                scrollbar_orientation::vertical,
                scrollbar.bounds,
                scrollbar.thumb,
                control_state);
        }
        if (layout.horizontal) {
            const classic_scrollbar_geometry scrollbar =
                horizontal_scrollbar(layout, control, metrics);
            control.draw_scrollbar(
                graphics,
                *painter,
                scrollbar_orientation::horizontal,
                scrollbar.bounds,
                scrollbar.thumb,
                control_state);
        }
        // The complete control frame is intentionally last. Rows, headers,
        // and scrollbar parts may touch an edge but must never erase it.
        const rect viewport_bounds = bounds;
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

    bool begin_table_scrollbar_drag(table_view &control,
                                    point position,
                                    bool &horizontal,
                                    int &grab_offset) {
        theme::metrics metrics;
        metrics.header_height = control._native_header_height;
        metrics.table_row_height = control._native_row_height;
        const geometry layout = table_geometry(control, metrics);
        if (layout.vertical) {
            const classic_scrollbar_geometry scrollbar =
                vertical_scrollbar(layout, control, metrics);
            if (scrollbar.thumb.contains(position)) {
                horizontal = false;
                grab_offset = position.y - scrollbar.thumb.y1();
                return true;
            }
        }
        if (layout.horizontal) {
            const classic_scrollbar_geometry scrollbar =
                horizontal_scrollbar(layout, control, metrics);
            if (scrollbar.thumb.contains(position)) {
                horizontal = true;
                grab_offset = position.x - scrollbar.thumb.x1();
                return true;
            }
        }
        return false;
    }

    bool drag_table_scrollbar(table_view &control,
                              point position,
                              bool horizontal,
                              int grab_offset) {
        theme::metrics metrics;
        metrics.header_height = control._native_header_height;
        metrics.table_row_height = control._native_row_height;
        const geometry layout = table_geometry(control, metrics);
        if (horizontal) {
            if (!layout.horizontal)
                return false;
            const classic_scrollbar_geometry scrollbar =
                horizontal_scrollbar(layout, control, metrics);
            const std::uint64_t value = classic_scrollbar_drag_value(
                scrollbar,
                scrollbar_orientation::horizontal,
                position.x,
                grab_offset,
                static_cast<std::uint64_t>(
                    std::max(1, layout.content_width)),
                static_cast<std::uint64_t>(
                    std::max(1, static_cast<int>(layout.body.d.w))));
            control.on_native_scroll(
                control.get_vertical_scroll_row(),
                static_cast<int>(std::min<std::uint64_t>(
                    value,
                    static_cast<std::uint64_t>(
                        std::numeric_limits<int>::max()))));
            return true;
        }
        if (!layout.vertical)
            return false;
        const classic_scrollbar_geometry scrollbar =
            vertical_scrollbar(layout, control, metrics);
        const std::uint64_t value = classic_scrollbar_drag_value(
            scrollbar,
            scrollbar_orientation::vertical,
            position.y,
            grab_offset,
            std::max<std::uint64_t>(
                1, control.get_display_row_count()),
            std::max<std::uint64_t>(
                1, control.get_visible_row_range().count));
        control.on_native_scroll(
            static_cast<std::size_t>(value),
            control.get_horizontal_scroll_offset());
        return true;
    }

    bool handle_table_click(table_view &control, point position) {
        // Emulated collection controls are painted into their root window
        // and deliberately have no graphics binding of their own. Hit-test
        // with the metrics synchronized during native creation instead of
        // trying to manufacture a child graphics context.
        theme::metrics metrics;
        metrics.header_height = control._native_header_height;
        metrics.table_row_height = control._native_row_height;
        const geometry layout = table_geometry(control, metrics);
        if (layout.vertical) {
            const classic_scrollbar_geometry scrollbar =
                vertical_scrollbar(layout, control, metrics);
            if (scrollbar.bounds.contains(position)) {
                std::size_t row = control.get_vertical_scroll_row();
                const std::size_t page = std::max<std::size_t>(
                    1, control.get_visible_row_range().count);
                if (scrollbar.decrement.contains(position)) {
                    row -= std::min<std::size_t>(row, 1);
                } else if (scrollbar.increment.contains(position)) {
                    ++row;
                } else if (position.y < scrollbar.thumb.y1()) {
                    row -= std::min(row, page);
                } else if (position.y >= scrollbar.thumb.y2()) {
                    row += page;
                }
                control.on_native_scroll(
                    row, control.get_horizontal_scroll_offset());
                return true;
            }
        }
        if (layout.horizontal) {
            const classic_scrollbar_geometry scrollbar =
                horizontal_scrollbar(layout, control, metrics);
            if (scrollbar.bounds.contains(position)) {
                int offset = control.get_horizontal_scroll_offset();
                const int step = std::max(8, layout.scrollbar);
                if (scrollbar.decrement.contains(position))
                    offset -= step;
                else if (scrollbar.increment.contains(position))
                    offset += step;
                else if (position.x < scrollbar.thumb.x1())
                    offset -= layout.body.d.w;
                else if (position.x >= scrollbar.thumb.x2())
                    offset += layout.body.d.w;
                control.on_native_scroll(
                    control.get_vertical_scroll_row(), offset);
                return true;
            }
        }
        if (position.y < layout.header) {
            int x = position.x +
                    control.get_horizontal_scroll_offset();
            for (const auto &column : control.get_columns()) {
                if (!column.visible)
                    continue;
                const int width = rendered_header_width(layout, column);
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
        const table_visible_range visible =
            control.get_visible_row_range();
        const std::uint64_t nominal_height =
            static_cast<std::uint64_t>(visible.count) *
            static_cast<std::uint64_t>(layout.row);
        const std::size_t offset =
            visible.count > 0 && nominal_height > layout.body.d.h
                ? std::min<std::size_t>(
                      visible.count - 1,
                      static_cast<std::size_t>(
                          static_cast<std::uint64_t>(
                              position.y - layout.body.p.y) *
                          visible.count /
                          std::max<int>(1, layout.body.d.h)))
                : static_cast<std::size_t>(
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
