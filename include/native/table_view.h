//
// Declares the advanced model-backed multi-column table control.
// The portable state supports virtual rows, grouping, searching,
// scrolling, and stable-ID row selection.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "collection_view.h"
#include "scrollbar.h"
#include "table_model.h"

namespace native
{
    class table_view;

    namespace detail
    {
        class control_render_access;
        class table_visible_rows;
        void draw_table_view(table_view &control, gpx &graphics);
        bool begin_table_scrollbar_drag(table_view &control,
                                        point position,
                                        bool &horizontal,
                                        int &grab_offset);
        bool drag_table_scrollbar(table_view &control,
                                  point position,
                                  bool horizontal,
                                  int grab_offset);
        bool handle_table_click(table_view &control, point position);
    }

    // Selects automatic, materialized, or virtual native adaptation.
    enum class table_data_mode
    {
        automatic,
        materialized,
        virtualized
    };

    // Selects single-row or multiple-row selection behavior.
    enum class table_selection_mode
    {
        single,
        multiple
    };

    // Selects which table cell separators are visible.
    enum class table_grid_lines : std::uint8_t
    {
        none = 0,
        horizontal = 1,
        vertical = 2,
        both = 3
    };

    // Describes a contiguous range of visible display rows.
    struct table_visible_range
    {
        std::size_t first = 0;
        std::size_t count = 0;
    };

    // Identifies one mapped display row without materializing cells.
    struct table_display_row
    {
        bool group = false;
        table_group_id group_id = 0;
        std::size_t model_row = 0;
    };

    // Selects one portable table keyboard-navigation action.
    enum class table_navigation
    {
        up,
        down,
        home,
        end,
        page_up,
        page_down,
        select_all,
        activate,
        toggle,
        collapse,
        expand
    };

    // Presents a virtual, multi-column row model as a native table.
    class table_view : public collection_view
    {
    public:
        // Construct a table view from scalar bounds.
        table_view(coord x = 0,
                   coord y = 0,
                   dim width = 320,
                   dim height = 240);

        // Construct a table view from position and dimensions.
        table_view(const point &position, const size &dimensions);

        // Construct a table view from complete bounds.
        explicit table_view(const rect &bounds);

        // Detach the borrowed model and destroy the native resource.
        ~table_view() override;

        // Install a borrowed model that must outlive this view.
        table_view &set_model(table_model *model);

        // Return the currently borrowed model, or null.
        table_model *get_model() const;

        // Replace column definitions after validating stable IDs.
        table_view &set_columns(std::vector<table_column> columns);

        // Return columns in current visual order.
        const std::vector<table_column> &get_columns() const;

        // Append one uniquely identified column.
        table_view &add_column(table_column column);

        // Append one column with builder syntax.
        table_view &operator<<(table_column column);

        // Remove one semantic column or throw std::out_of_range.
        table_view &remove_column(table_column_id id);

        // Move a column to a visual index without emitting a signal.
        table_view &move_column(table_column_id id,
                                std::size_t display_index);

        // Set and clamp a column width without emitting a signal.
        table_view &set_column_width(table_column_id id, dim width);

        // Set semantic column visibility without emitting a signal.
        table_view &set_column_visible(table_column_id id,
                                       bool visible);

        // Show or hide the native table heading.
        table_view &set_header_visible(bool visible);

        // Return whether the heading is visible.
        bool get_header_visible() const;

        // Enable or disable user-driven column reordering.
        table_view &set_columns_reorderable(bool reorderable);

        // Return whether user-driven column reordering is enabled.
        bool get_columns_reorderable() const;

        // Enable or disable user-driven column resizing.
        table_view &set_columns_resizable(bool resizable);

        // Return whether user-driven column resizing is enabled.
        bool get_columns_resizable() const;

        // Stretch the trailing visible column into unused viewport width.
        table_view &set_fill_last_column(bool fill);

        // Return whether the trailing visible column fills unused width.
        bool get_fill_last_column() const;

        // Enable a native column-visibility menu where available.
        table_view &set_column_visibility_menu_enabled(bool enabled);

        // Return whether a column-visibility menu is enabled.
        bool get_column_visibility_menu_enabled() const;

        // Set the displayed sort indicator without sorting the model.
        table_view &set_sort(std::optional<table_sort> sort);

        // Return the displayed sort indicator.
        std::optional<table_sort> get_sort() const;

        // Select automatic, materialized, or virtual adaptation.
        table_view &set_data_mode(table_data_mode mode);

        // Return the requested data-adaptation mode.
        table_data_mode get_data_mode() const;

        // Select single-row or multiple-row selection.
        table_view &set_selection_mode(table_selection_mode mode);

        // Return the current selection mode.
        table_selection_mode get_selection_mode() const;

        // Return selected stable row IDs in logical row order.
        std::vector<table_row_id> get_selected_rows() const;

        // Replace logical selection without emitting a user signal.
        table_view &set_selected_rows(
            const std::vector<table_row_id> &rows);

        // Programmatically set a group's transient expansion state.
        table_view &set_group_expanded(table_group_id id,
                                       bool expanded);

        // Return a group's current expansion state.
        bool get_group_expanded(table_group_id id) const;

        // Enable or disable native alternating-row appearance.
        table_view &set_alternating_rows(bool enabled);

        // Return whether alternating rows are enabled.
        bool get_alternating_rows() const;

        // Select visible horizontal and vertical cell separators.
        table_view &set_grid_lines(table_grid_lines lines);

        // Return the selected cell-separator policy.
        table_grid_lines get_grid_lines() const;

        // Set an optional row height; null uses the native default.
        table_view &set_row_height(std::optional<dim> height);

        // Return the optional explicit row height.
        std::optional<dim> get_row_height() const;

        // Set an optional cell icon size; null uses native defaults.
        table_view &set_icon_size(std::optional<size> dimensions);

        // Return the optional explicit cell icon size.
        std::optional<size> get_icon_size() const;

        // Set vertical scrollbar visibility policy.
        table_view &set_vertical_scrollbar_policy(
            scrollbar_policy policy);

        // Return vertical scrollbar visibility policy.
        scrollbar_policy get_vertical_scrollbar_policy() const;

        // Set horizontal scrollbar visibility policy.
        table_view &set_horizontal_scrollbar_policy(
            scrollbar_policy policy);

        // Return horizontal scrollbar visibility policy.
        scrollbar_policy get_horizontal_scrollbar_policy() const;

        // Enable or disable native incremental type search.
        table_view &set_type_search_enabled(bool enabled);

        // Return whether incremental type search is enabled.
        bool get_type_search_enabled() const;

        // Find a stable row without changing view state.
        std::optional<table_row_id>
        find(const table_search &query) const;

        // Find text using portable substring search by default.
        std::optional<table_row_id>
        find_text(const std::string &text,
                  table_search_match match =
                      table_search_match::substring) const;

        // Find and select a row without changing group expansion.
        bool find_and_select(const table_search &query);

        // Find, expand, select, and reveal a logical row.
        bool find_and_reveal(const table_search &query);

        // Scroll the row to the leading edge where practical.
        table_view &scroll_to_row(table_row_id id);

        // Scroll only enough to make the row visible.
        table_view &ensure_row_visible(table_row_id id);

        // Return the first visible data row, if any.
        std::optional<table_row_id> get_first_visible_row() const;

        // Return the last visible data row, if any.
        std::optional<table_row_id> get_last_visible_row() const;

        // Return the current approximate display-row viewport.
        table_visible_range get_visible_row_range() const;

        // Return the compact mapped number of display rows.
        std::size_t get_display_row_count() const;

        // Return one mapped display row or throw std::out_of_range.
        table_display_row get_display_row(
            std::size_t display_index) const;

        // Return the current horizontal content offset in pixels.
        int get_horizontal_scroll_offset() const;

        // Return the current vertical display-row offset.
        std::size_t get_vertical_scroll_row() const;

        // Accept a native user-originated row selection.
        virtual void on_native_selection(
            const std::vector<table_row_id> &rows);

        // Accept a native user-originated row activation.
        virtual void on_native_activate(table_row_id id);

        // Accept a native sortable-column heading action.
        virtual void on_native_sort_request(table_column_id column);

        // Accept a native user-originated column resize.
        virtual void on_native_column_resize(table_column_id column,
                                             dim width);

        // Accept a native user-originated column move.
        virtual void on_native_column_move(
            table_column_id column,
            std::size_t display_index);

        // Accept a native user-originated group expansion change.
        virtual void on_native_group_expand(table_group_id group,
                                            bool expanded);

        // Accept a native vertical and horizontal scroll position.
        virtual void on_native_scroll(
            std::size_t first_display_row,
            int horizontal_offset);

        // Apply one backend-originated keyboard navigation action.
        virtual void on_native_navigation(
            table_navigation navigation,
            bool extend = false);

        // Apply one native UTF-8 incremental type-search fragment.
        virtual void on_native_type_text(const std::string &text);

    protected:
        // Create the backend table resource.
        void create_native() override;

        // Destroy the backend table resource.
        void destroy_native() override;

        // Show the backend table resource.
        void show_native() override;

    public:

        // Emits selected row IDs after a user-originated change.
        signal<const std::vector<table_row_id> &>
            on_selection_change;

        // Emits a stable row ID after user activation.
        signal<table_row_id> on_row_activate;

        // Emits a proposed user sort descriptor.
        signal<table_sort> on_sort_request;

        // Emits a user-resized semantic column and width.
        signal<table_column_id, dim> on_column_resize;

        // Emits a user-moved semantic column and visual index.
        signal<table_column_id, std::size_t> on_column_move;

        // Emits a user-changed group expansion state.
        signal<table_group_id, bool> on_group_expand;

    protected:
        // Clamp scroll positions after a bounds change.
        void on_bounds_changed() override;

        // Draw the complete table background before its child parts.
        virtual void draw_background(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        // Draw the final content-viewport relief after every table part.
        // Native scrollbar reservations are outside these bounds.
        virtual void draw_border(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        // Draw one column-heading background.
        virtual void draw_header_background(
            gpx &graphics,
            theme &appearance,
            const table_column &column,
            const rect &bounds,
            const theme::state &state);

        // Draw one column heading's title and sort indicator.
        virtual void draw_header_content(
            gpx &graphics,
            theme &appearance,
            const table_column &column,
            const rect &bounds,
            const theme::state &state);

        // Draw one column heading's separators or border.
        virtual void draw_header_border(
            gpx &graphics,
            theme &appearance,
            const table_column &column,
            const rect &bounds,
            const theme::state &state);

        // Draw one group heading, including its disclosure control.
        virtual void draw_group(
            gpx &graphics,
            theme &appearance,
            const table_group &group,
            const rect &bounds,
            const theme::state &state);

        // Draw a data row's background and selection.
        virtual void draw_row_background(
            gpx &graphics,
            theme &appearance,
            table_row_id row,
            std::size_t model_row,
            const rect &bounds,
            const theme::state &state);

        // Draw one data cell's background before its content.
        virtual void draw_cell_background(
            gpx &graphics,
            theme &appearance,
            table_row_id row,
            std::size_t model_row,
            const table_column &column,
            const table_cell &cell,
            const rect &bounds,
            const theme::state &state);

        // Draw one data cell's image and text content.
        virtual void draw_cell_content(
            gpx &graphics,
            theme &appearance,
            table_row_id row,
            std::size_t model_row,
            const table_column &column,
            const table_cell &cell,
            const rect &bounds,
            const theme::state &state);

        // Draw one data cell's configured border or separator.
        virtual void draw_cell_border(
            gpx &graphics,
            theme &appearance,
            table_row_id row,
            std::size_t model_row,
            const table_column &column,
            const table_cell &cell,
            const rect &bounds,
            const theme::state &state);

        // Draw the keyboard focus around a completed data row.
        virtual void draw_row_focus(
            gpx &graphics,
            theme &appearance,
            table_row_id row,
            std::size_t model_row,
            const rect &bounds,
            const theme::state &state);

        // Draw one complete scrollbar track and thumb.
        virtual void draw_scrollbar(
            gpx &graphics,
            theme &appearance,
            scrollbar_orientation orientation,
            const rect &track,
            const rect &thumb,
            const theme::state &state);

        // Apply all cached model and appearance state natively.
        virtual void apply_table();

        // Apply cached logical selection natively.
        virtual void apply_selection();

        // Apply cached scroll positions natively.
        virtual void apply_scroll();

        // Refresh metrics obtained from the active native theme.
        void synchronize_theme_metrics() override;

    private:
        friend class detail::control_render_access;
        friend void detail::draw_table_view(
            table_view &control, gpx &graphics);
        friend bool detail::begin_table_scrollbar_drag(
            table_view &control,
            point position,
            bool &horizontal,
            int &grab_offset);
        friend bool detail::drag_table_scrollbar(
            table_view &control,
            point position,
            bool horizontal,
            int grab_offset);
        friend bool detail::handle_table_click(
            table_view &control, point position);

        table_model *_model = nullptr;
        int _model_connection = 0;
        std::vector<table_column> _columns;
        std::vector<table_row_id> _selection;
        std::optional<table_sort> _sort;
        std::optional<dim> _row_height;
        std::optional<size> _icon_size;
        std::unique_ptr<detail::table_visible_rows> _visible_rows;
        table_data_mode _data_mode = table_data_mode::automatic;
        table_selection_mode _selection_mode =
            table_selection_mode::single;
        scrollbar_policy _vertical_policy =
            scrollbar_policy::automatic;
        scrollbar_policy _horizontal_policy =
            scrollbar_policy::automatic;
        table_grid_lines _grid_lines = table_grid_lines::none;
        bool _header_visible = true;
        bool _columns_reorderable = true;
        bool _columns_resizable = true;
        bool _fill_last_column = true;
        bool _visibility_menu = false;
        bool _alternating_rows = false;
        bool _type_search = true;
        std::size_t _vertical_row = 0;
        int _horizontal_offset = 0;
        table_row_id _focused_row = invalid_table_row_id;
        std::optional<std::size_t> _focused_model_row;
        int _native_row_height = 20;
        int _native_header_height = 24;
        std::string _type_search_buffer;
        std::uint64_t _type_search_deadline = 0;

        void rebuild_visible_rows();
        bool handle_model_change(const table_model_change &change);
        std::optional<std::size_t> model_row_for_id(
            table_row_id id) const;
        std::optional<std::size_t> display_row_for_id(
            table_row_id id) const;
        std::optional<std::size_t> find_model_row(
            const table_search &query) const;
        table_row_id row_id_for_model_row(
            std::size_t model_row) const;
        void select_known_row(std::size_t model_row, bool emit);
        void ensure_model_row_visible(std::size_t model_row);
        std::size_t rows_per_page(bool include_partial = false) const;
        void validate_columns(
            const std::vector<table_column> &columns) const;
        void validate_selection(
            const std::vector<table_row_id> &rows) const;
    };
} // namespace native
