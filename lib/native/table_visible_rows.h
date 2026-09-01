//
// Declares the compact logical-to-display row mapper shared by native
// and custom table adapters. It stores groups, never individual rows.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstddef>
#include <optional>
#include <unordered_map>
#include <vector>

#include <native/table_view.h>

namespace native::detail
{
    // Maps grouped logical rows to display rows without materialization.
    class table_visible_rows
    {
    public:
        // Construct an empty mapper.
        table_visible_rows();

        // Rebuild compact metadata while preserving known expansion.
        void reset(table_model *model);

        // Return the number of data and group heading display rows.
        std::size_t display_row_count() const;

        // Return one display-row mapping or throw std::out_of_range.
        table_display_row row_at(std::size_t display_index) const;

        // Return a display index for a visible logical model row.
        std::optional<std::size_t>
        display_index_for_model_row(std::size_t model_row) const;

        // Return a logical model row for a non-group display row.
        std::optional<std::size_t>
        model_row_for_display_index(std::size_t display_index) const;

        // Return the group containing a logical row, if any.
        std::optional<table_group>
        group_for_model_row(std::size_t model_row) const;

        // Set a known group's expansion state.
        void set_expanded(table_group_id id, bool expanded);

        // Return a known group's expansion state.
        bool get_expanded(table_group_id id) const;

        // Return validated groups in logical order.
        const std::vector<table_group> &groups() const;

    private:
        table_model *_model = nullptr;
        std::size_t _row_count = 0;
        std::vector<table_group> _groups;
        std::unordered_map<table_group_id, bool> _expanded;

        bool expanded(const table_group &group) const;
    };
} // namespace native::detail
