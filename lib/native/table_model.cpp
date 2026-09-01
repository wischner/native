//
// Implements default table grouping and lazy portable row search.
// Models can override find() to use an application or database index.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/table_model.h>

#include <algorithm>
#include <stdexcept>

#include "table_search.h"

namespace native
{
    table_model::~table_model() = default;

    std::size_t table_model::group_count() const {
        return 0;
    }

    table_group table_model::group(std::size_t group_index) const {
        (void)group_index;
        throw std::out_of_range("table model has no groups");
    }

    std::optional<std::size_t>
    table_model::find(const table_search &query) const {
        const std::size_t count = row_count();
        if (count == 0 || query.columns.empty())
            return std::nullopt;

        const std::size_t start = std::min(query.start_row, count);
        const auto matches = [&](std::size_t row) {
            return std::any_of(
                query.columns.begin(), query.columns.end(),
                [&](table_column_id column) {
                    return detail::table_text_matches(
                        cell(row, column).text,
                        query.text,
                        query.match,
                        query.case_mode);
                });
        };
        for (std::size_t row = start; row < count; ++row) {
            if (matches(row))
                return row;
        }
        if (query.wrap) {
            for (std::size_t row = 0; row < start; ++row) {
                if (matches(row))
                    return row;
            }
        }
        return std::nullopt;
    }
} // namespace native
