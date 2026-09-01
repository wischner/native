//
// Declares the virtual row model and portable value types used by
// table_view. Stable IDs keep selection independent of display order.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "geometry.h"
#include "signal.h"

namespace native
{
    class img;

    // Stable semantic identifiers used by a table model.
    using table_row_id = std::uint64_t;
    using table_group_id = std::uint64_t;
    using table_column_id = std::uint32_t;

    // Reserved value representing the absence of a row.
    constexpr table_row_id invalid_table_row_id = 0;

    // Selects horizontal content alignment inside a table column.
    enum class table_alignment
    {
        start,
        center,
        end
    };

    // Defines one semantic table column and its display constraints.
    struct table_column
    {
        table_column_id id = 0;
        std::string title;
        dim width = 120;
        dim min_width = 24;
        dim max_width = 32767;
        table_alignment alignment = table_alignment::start;
        bool visible = true;
        bool resizable = true;
        bool reorderable = true;
        bool sortable = false;
        bool allow_image = true;
    };

    // Contains display text and an optional borrowed image for one cell.
    struct table_cell
    {
        std::string text;
        const img *image = nullptr;
    };

    // Selects the direction displayed by a table sort indicator.
    enum class sort_direction
    {
        ascending,
        descending
    };

    // Identifies a requested table sort column and direction.
    struct table_sort
    {
        table_column_id column = 0;
        sort_direction direction = sort_direction::ascending;

        // Compare two sort descriptions by semantic value.
        bool operator==(const table_sort &) const = default;
    };

    // Describes one contiguous group of logical model rows.
    struct table_group
    {
        table_group_id id = 0;
        std::string title;
        std::size_t first_row = 0;
        std::size_t row_count = 0;
        bool collapsible = false;
        bool expanded = true;
    };

    // Selects exact, leading, or contained text matching.
    enum class table_search_match
    {
        exact,
        prefix,
        substring
    };

    // Selects whether portable table search folds letter case.
    enum class table_search_case
    {
        sensitive,
        insensitive
    };

    // Describes a logical row search over selected semantic columns.
    struct table_search
    {
        std::string text;
        table_search_match match = table_search_match::substring;
        table_search_case case_mode = table_search_case::insensitive;
        std::vector<table_column_id> columns;
        std::size_t start_row = 0;
        bool wrap = true;
    };

    // Selects the kind of incremental model invalidation.
    enum class table_model_change_kind
    {
        reset,
        rows_inserted,
        rows_removed,
        rows_changed,
        groups_changed
    };

    // Describes one contiguous logical-row model change.
    struct table_model_change
    {
        table_model_change_kind kind =
            table_model_change_kind::reset;
        std::size_t first = 0;
        std::size_t count = 0;
    };

    // Supplies stable, lazily requested rows to a table_view.
    class table_model
    {
    public:
        // Destroy the model after detaching it from every table view.
        virtual ~table_model();

        // Return the logical number of data rows.
        virtual std::size_t row_count() const = 0;

        // Return a stable non-zero ID for a logical row index.
        virtual table_row_id row_id(std::size_t row) const = 0;

        // Return display content for a row and semantic column.
        virtual table_cell cell(std::size_t row,
                                table_column_id column) const = 0;

        // Return the number of contiguous row groups.
        virtual std::size_t group_count() const;

        // Return group metadata or throw std::out_of_range.
        virtual table_group group(std::size_t group_index) const;

        // Find a logical row using a lazy portable scan by default.
        virtual std::optional<std::size_t>
        find(const table_search &query) const;

        // Notify attached views after model contents or order change.
        signal<const table_model_change &> on_change;
    };
} // namespace native
