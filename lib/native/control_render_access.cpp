//
// Dispatches backend painting to protected virtual control stages.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "control_render_access.h"

#include <algorithm>

#include <native/button.h>
#include <native/check.h>
#include <native/code_edit.h>
#include <native/combo_box.h>
#include <native/icon_view.h>
#include <native/list.h>
#include <native/radio.h>
#include <native/text_edit.h>
#include <native/table_view.h>
#include <native/tree_view.h>

namespace native::detail
{
    void control_render_access::draw(
        combo_box &control,
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        control.draw_control(graphics, appearance, bounds, state);
    }

    void control_render_access::draw(
        button &control,
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        control.draw_control(graphics, appearance, bounds, state);
    }

    void control_render_access::draw(
        check &control,
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        control.draw_control(graphics, appearance, bounds, state);
    }

    void control_render_access::draw(
        radio &control,
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        control.draw_control(graphics, appearance, bounds, state);
    }

    void control_render_access::draw(
        list &control,
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        control.draw_control(graphics, appearance, bounds, state);
    }

    void control_render_access::draw(
        text_edit &control,
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        control.draw_control(graphics, appearance, bounds, state);
    }

    void control_render_access::draw_icon_background(
        icon_view &control,
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        control.draw_background(
            graphics, appearance, bounds, state);
    }

    void control_render_access::draw_icon_item(
        icon_view &control,
        gpx &graphics,
        theme &appearance,
        std::size_t index,
        const icon_view_item &item,
        const rect &bounds,
        const theme::state &state) {
        const theme::metrics metrics = appearance.defaults();
        const size icon_size = control.get_icon_size();
        const int padding = metrics.icon_view_padding_x;
        rect image_box;
        point label_position;
        if (control.get_label_mode() ==
            icon_view_label_mode::beside) {
            image_box = rect(bounds.p.x + padding,
                             bounds.p.y + padding,
                             icon_size.w,
                             icon_size.h);
            label_position = point(
                static_cast<coord>(image_box.x2() + padding),
                static_cast<coord>(bounds.p.y + padding));
        } else {
            image_box = rect(
                static_cast<coord>(
                    bounds.p.x +
                    std::max(0,
                             (static_cast<int>(bounds.d.w) -
                              static_cast<int>(icon_size.w)) /
                                 2)),
                bounds.p.y + padding,
                icon_size.w,
                icon_size.h);
            label_position = point(
                bounds.p.x + padding,
                static_cast<coord>(image_box.y2() + padding));
        }

        control.draw_item_background(
            graphics, appearance, index, item, bounds, state);
        if (item.image) {
            const double scale = std::min(
                {1.0,
                 static_cast<double>(image_box.d.w) / item.image->w(),
                 static_cast<double>(image_box.d.h) / item.image->h()});
            const int width = std::max(
                1, static_cast<int>(item.image->w() * scale));
            const int height = std::max(
                1, static_cast<int>(item.image->h() * scale));
            const rect image_bounds(
                static_cast<coord>(
                    image_box.p.x +
                    (static_cast<int>(image_box.d.w) - width) / 2),
                static_cast<coord>(
                    image_box.p.y +
                    (static_cast<int>(image_box.d.h) - height) / 2),
                static_cast<dim>(width),
                static_cast<dim>(height));
            control.draw_item_image(
                graphics,
                appearance,
                index,
                item,
                image_bounds,
                state);
        }
        if (control.get_label_mode() !=
            icon_view_label_mode::hidden) {
            const int label_width = std::max(
                0, bounds.x2() - label_position.x - padding);
            const int label_height = std::max(
                0, bounds.y2() - label_position.y - padding);
            control.draw_item_label(
                graphics,
                appearance,
                index,
                item,
                rect(label_position,
                     size(static_cast<dim>(label_width),
                          static_cast<dim>(label_height))),
                state);
        }
        control.draw_item_focus(
            graphics, appearance, index, item, bounds, state);
    }

    void control_render_access::draw_text_content(
        code_edit &control,
        gpx &graphics,
        theme &appearance,
        const text_span &span,
        const std::string &display,
        point position,
        rgba foreground,
        bool bold,
        const theme::state &state) {
        control.draw_text_content(
            graphics,
            appearance,
            span,
            display,
            position,
            foreground,
            bold,
            state);
    }

    void control_render_access::draw_completion_background(
        code_edit &control,
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        control.draw_completion_background(
            graphics, appearance, bounds, state);
    }

    void control_render_access::draw_completion_item(
        code_edit &control,
        gpx &graphics,
        theme &appearance,
        std::size_t index,
        const completion_item &item,
        const rect &bounds,
        const theme::state &state) {
        control.draw_completion_item(
            graphics, appearance, index, item, bounds, state);
    }

    void control_render_access::draw_table_group(
        table_view &control,
        gpx &graphics,
        theme &appearance,
        const table_group &group,
        const rect &bounds,
        const theme::state &state) {
        control.draw_group(
            graphics, appearance, group, bounds, state);
    }

    void control_render_access::draw_table_row_background(
        table_view &control,
        gpx &graphics,
        theme &appearance,
        std::uint64_t row,
        std::size_t model_row,
        const rect &bounds,
        const theme::state &state) {
        control.draw_row_background(
            graphics, appearance, row, model_row, bounds, state);
    }

    void control_render_access::draw_table_cell(
        table_view &control,
        gpx &graphics,
        theme &appearance,
        std::uint64_t row,
        std::size_t model_row,
        const table_column &column,
        const table_cell &cell,
        const rect &bounds,
        const theme::state &state) {
        control.draw_cell_background(
            graphics,
            appearance,
            row,
            model_row,
            column,
            cell,
            bounds,
            state);
        control.draw_cell_content(
            graphics,
            appearance,
            row,
            model_row,
            column,
            cell,
            bounds,
            state);
        control.draw_cell_border(
            graphics,
            appearance,
            row,
            model_row,
            column,
            cell,
            bounds,
            state);
    }

    void control_render_access::draw_table_row_focus(
        table_view &control,
        gpx &graphics,
        theme &appearance,
        std::uint64_t row,
        std::size_t model_row,
        const rect &bounds,
        const theme::state &state) {
        control.draw_row_focus(
            graphics, appearance, row, model_row, bounds, state);
    }

    void control_render_access::draw_table_header(
        table_view &control,
        gpx &graphics,
        theme &appearance,
        const table_column &column,
        const rect &bounds,
        const theme::state &state) {
        control.draw_header_background(
            graphics, appearance, column, bounds, state);
        control.draw_header_content(
            graphics, appearance, column, bounds, state);
        control.draw_header_border(
            graphics, appearance, column, bounds, state);
    }

    void control_render_access::draw_tree_row(
        tree_view &control,
        gpx &graphics,
        theme &appearance,
        std::size_t visible_index,
        const tree_view_item &item,
        std::size_t depth,
        const rect &bounds,
        const theme::state &state,
        bool draw_hierarchy) {
        const tree_view_visible_item visible{item.id, depth};
        const theme::metrics metrics = appearance.defaults();
        control.draw_row_background(
            graphics,
            appearance,
            visible,
            item,
            bounds,
            state);
        const rect portable_row = control.get_row_bounds(visible_index);
        const rect portable_disclosure =
            control.get_disclosure_bounds(visible_index);
        const rect disclosure(
            point(
                static_cast<coord>(
                    bounds.p.x + portable_disclosure.p.x -
                    portable_row.p.x),
                static_cast<coord>(
                    bounds.p.y + portable_disclosure.p.y -
                    portable_row.p.y)),
            portable_disclosure.d);
        if (draw_hierarchy) {
            control.draw_connectors(
                graphics,
                appearance,
                visible,
                item,
                bounds,
                disclosure,
                state);
            control.draw_disclosure(
                graphics,
                appearance,
                visible,
                item,
                disclosure,
                state);
        }
        int content_x = disclosure.x2() + metrics.header_gap;
        if (item.image) {
            const size requested = control.get_icon_size();
            const int requested_width = std::max<int>(1, requested.w);
            const int requested_height = std::max<int>(1, requested.h);
            const double scale = std::min(
                {1.0,
                 static_cast<double>(requested_width) / item.image->w(),
                 static_cast<double>(requested_height) / item.image->h()});
            const int width = std::max(
                1, static_cast<int>(item.image->w() * scale));
            const int height = std::max(
                1, static_cast<int>(item.image->h() * scale));
            const rect image_bounds(
                static_cast<coord>(content_x +
                    (requested_width - width) / 2),
                static_cast<coord>(bounds.p.y +
                    (static_cast<int>(bounds.d.h) - height) / 2),
                static_cast<dim>(width),
                static_cast<dim>(height));
            control.draw_item_image(
                graphics,
                appearance,
                visible,
                item,
                image_bounds,
                state);
            content_x += requested_width + metrics.header_gap;
        }
        const int text_width = std::max(
            0,
            bounds.x2() - content_x - metrics.header_padding_x);
        control.draw_item_text(
            graphics,
            appearance,
            visible,
            item,
            rect(static_cast<coord>(content_x),
                 bounds.p.y,
                 static_cast<dim>(text_width),
                 bounds.d.h),
            state);
        control.draw_row_focus(
            graphics,
            appearance,
            visible,
            item,
            bounds,
            state);
    }
} // namespace native::detail
