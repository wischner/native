//
// Declares the materialized convenience model for small and medium
// tables. Cells retain their strings and borrow optional images.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include "table_model.h"

namespace native
{
    // Associates a semantic column ID with one stored table cell.
    struct table_store_cell
    {
        table_column_id column = 0;
        table_cell value;
    };

    // Stores one stable row and its sparse semantic cells.
    struct table_store_row
    {
        table_row_id id = invalid_table_row_id;
        std::vector<table_store_cell> cells;
    };

    // Owns a materialized row collection implementing table_model.
    class table_store : public table_model
    {
    public:
        // Construct an empty table store.
        table_store();

        // Construct a table store from rows and optional groups.
        table_store(std::vector<table_store_row> rows,
                    std::vector<table_group> groups = {});

        // Return the number of stored rows.
        std::size_t row_count() const override;

        // Return a row's stable ID or throw std::out_of_range.
        table_row_id row_id(std::size_t row) const override;

        // Return a stored cell, or an empty cell when absent.
        table_cell cell(std::size_t row,
                        table_column_id column) const override;

        // Return the number of stored groups.
        std::size_t group_count() const override;

        // Return one stored group or throw std::out_of_range.
        table_group group(std::size_t group_index) const override;

        // Return all materialized rows in logical order.
        const std::vector<table_store_row> &get_rows() const;

        // Replace all rows and emit one model reset.
        table_store &set_rows(std::vector<table_store_row> rows);

        // Append a row and emit one insertion notification.
        table_store &add_row(table_store_row row);

        // Insert a row at a logical index and emit one notification.
        table_store &insert_row(std::size_t index,
                                table_store_row row);

        // Replace one row without changing its logical position.
        table_store &set_row(std::size_t index,
                             table_store_row row);

        // Remove one row and emit one removal notification.
        table_store &remove_row(std::size_t index);

        // Remove every row and group and emit one model reset.
        table_store &clear();

        // Replace contiguous group metadata and notify attached views.
        table_store &set_groups(std::vector<table_group> groups);

        // Return all stored group descriptions.
        const std::vector<table_group> &get_groups() const;

    private:
        std::vector<table_store_row> _rows;
        std::vector<table_group> _groups;

        void validate_rows(
            const std::vector<table_store_row> &rows) const;
        void validate_groups(
            const std::vector<table_group> &groups) const;
    };
} // namespace native
