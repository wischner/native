//
// Provides backend painters with narrow access to protected control
// template methods without exposing those methods in the public API.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include <native/theme.h>

namespace native
{
    class button;
    class check;
    class code_edit;
    class combo_box;
    struct completion_item;
    class icon_view;
    struct icon_view_item;
    class list;
    class radio;
    class text_edit;
    class table_view;
    struct text_span;
    struct table_cell;
    struct table_column;
    struct table_group;
    class tree_view;
    struct tree_view_item;

    namespace detail
    {
        class control_render_access
        {
        public:
            static void draw(button &control,
                             gpx &graphics,
                             theme &appearance,
                             const rect &bounds,
                             const theme::state &state);

            static void draw(check &control,
                             gpx &graphics,
                             theme &appearance,
                             const rect &bounds,
                             const theme::state &state);

            static void draw(combo_box &control,
                             gpx &graphics,
                             theme &appearance,
                             const rect &bounds,
                             const theme::state &state);

            static void draw(radio &control,
                             gpx &graphics,
                             theme &appearance,
                             const rect &bounds,
                             const theme::state &state);

            static void draw(list &control,
                             gpx &graphics,
                             theme &appearance,
                             const rect &bounds,
                             const theme::state &state);

            static void draw(text_edit &control,
                             gpx &graphics,
                             theme &appearance,
                             const rect &bounds,
                             const theme::state &state);

            static void draw_icon_background(
                icon_view &control,
                gpx &graphics,
                theme &appearance,
                const rect &bounds,
                const theme::state &state);

            static void draw_icon_item(
                icon_view &control,
                gpx &graphics,
                theme &appearance,
                std::size_t index,
                const icon_view_item &item,
                const rect &bounds,
                const theme::state &state);

            static void draw_text_content(
                code_edit &control,
                gpx &graphics,
                theme &appearance,
                const text_span &span,
                const std::string &display,
                point position,
                rgba foreground,
                bool bold,
                const theme::state &state);

            static void draw_completion_background(
                code_edit &control,
                gpx &graphics,
                theme &appearance,
                const rect &bounds,
                const theme::state &state);

            static void draw_completion_item(
                code_edit &control,
                gpx &graphics,
                theme &appearance,
                std::size_t index,
                const completion_item &item,
                const rect &bounds,
                const theme::state &state);

            static void draw_table_group(
                table_view &control,
                gpx &graphics,
                theme &appearance,
                const table_group &group,
                const rect &bounds,
                const theme::state &state);

            static void draw_table_row_background(
                table_view &control,
                gpx &graphics,
                theme &appearance,
                std::uint64_t row,
                std::size_t model_row,
                const rect &bounds,
                const theme::state &state);

            static void draw_table_cell(
                table_view &control,
                gpx &graphics,
                theme &appearance,
                std::uint64_t row,
                std::size_t model_row,
                const table_column &column,
                const table_cell &cell,
                const rect &bounds,
                const theme::state &state);

            static void draw_table_row_focus(
                table_view &control,
                gpx &graphics,
                theme &appearance,
                std::uint64_t row,
                std::size_t model_row,
                const rect &bounds,
                const theme::state &state);

            static void draw_table_header(
                table_view &control,
                gpx &graphics,
                theme &appearance,
                const table_column &column,
                const rect &bounds,
                const theme::state &state);

            static void draw_tree_row(
                tree_view &control,
                gpx &graphics,
                theme &appearance,
                std::size_t visible_index,
                const tree_view_item &item,
                std::size_t depth,
                const rect &bounds,
                const theme::state &state);
        };
    } // namespace detail
} // namespace native
