//
// Implements the materialized table model and precise incremental
// notifications used by table_view adapters.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/table_store.h>

#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace native
{
    table_store::table_store() = default;

    table_store::table_store(std::vector<table_store_row> rows,
                             std::vector<table_group> groups)
        : _rows(std::move(rows))
        , _groups(std::move(groups)) {
        validate_rows(_rows);
        validate_groups(_groups);
    }

    std::size_t table_store::row_count() const {
        return _rows.size();
    }

    table_row_id table_store::row_id(std::size_t row) const {
        return _rows.at(row).id;
    }

    table_cell table_store::cell(std::size_t row,
                                 table_column_id column) const {
        const auto &cells = _rows.at(row).cells;
        const auto found = std::find_if(
            cells.begin(), cells.end(),
            [column](const table_store_cell &candidate) {
                return candidate.column == column;
            });
        return found == cells.end() ? table_cell{} : found->value;
    }

    std::size_t table_store::group_count() const {
        return _groups.size();
    }

    table_group table_store::group(std::size_t group_index) const {
        return _groups.at(group_index);
    }

    const std::vector<table_store_row> &table_store::get_rows() const {
        return _rows;
    }

    table_store &table_store::set_rows(
        std::vector<table_store_row> rows) {
        validate_rows(rows);
        _rows = std::move(rows);
        _groups.clear();
        table_model_change change;
        change.kind = table_model_change_kind::reset;
        on_change.emit(change);
        return *this;
    }

    table_store &table_store::add_row(table_store_row row) {
        return insert_row(_rows.size(), std::move(row));
    }

    table_store &table_store::insert_row(std::size_t index,
                                         table_store_row row) {
        if (index > _rows.size())
            throw std::out_of_range("table row index is out of range");
        std::vector<table_store_row> candidate = _rows;
        candidate.insert(candidate.begin() +
                             static_cast<std::ptrdiff_t>(index),
                         std::move(row));
        validate_rows(candidate);
        for (auto &group : _groups) {
            const std::size_t end = group.first_row + group.row_count;
            if (index < group.first_row)
                ++group.first_row;
            else if (index < end)
                ++group.row_count;
        }
        _rows = std::move(candidate);
        table_model_change change;
        change.kind = table_model_change_kind::rows_inserted;
        change.first = index;
        change.count = 1;
        on_change.emit(change);
        return *this;
    }

    table_store &table_store::set_row(std::size_t index,
                                      table_store_row row) {
        if (index >= _rows.size())
            throw std::out_of_range("table row index is out of range");
        std::vector<table_store_row> candidate = _rows;
        candidate[index] = std::move(row);
        validate_rows(candidate);
        _rows = std::move(candidate);
        table_model_change change;
        change.kind = table_model_change_kind::rows_changed;
        change.first = index;
        change.count = 1;
        on_change.emit(change);
        return *this;
    }

    table_store &table_store::remove_row(std::size_t index) {
        if (index >= _rows.size())
            throw std::out_of_range("table row index is out of range");
        _rows.erase(_rows.begin() +
                    static_cast<std::ptrdiff_t>(index));
        for (auto &group : _groups) {
            const std::size_t end = group.first_row + group.row_count;
            if (index < group.first_row)
                --group.first_row;
            else if (index < end)
                --group.row_count;
        }
        table_model_change change;
        change.kind = table_model_change_kind::rows_removed;
        change.first = index;
        change.count = 1;
        on_change.emit(change);
        return *this;
    }

    table_store &table_store::clear() {
        _rows.clear();
        _groups.clear();
        table_model_change change;
        change.kind = table_model_change_kind::reset;
        on_change.emit(change);
        return *this;
    }

    table_store &table_store::set_groups(
        std::vector<table_group> groups) {
        validate_groups(groups);
        _groups = std::move(groups);
        table_model_change change;
        change.kind = table_model_change_kind::groups_changed;
        on_change.emit(change);
        return *this;
    }

    const std::vector<table_group> &table_store::get_groups() const {
        return _groups;
    }

    void table_store::validate_rows(
        const std::vector<table_store_row> &rows) const {
        std::unordered_set<table_row_id> ids;
        for (const auto &row : rows) {
            if (row.id == invalid_table_row_id ||
                !ids.insert(row.id).second) {
                throw std::invalid_argument(
                    "table row IDs must be non-zero and unique");
            }
            std::unordered_set<table_column_id> columns;
            for (const auto &cell_value : row.cells) {
                if (cell_value.column == 0 ||
                    !columns.insert(cell_value.column).second) {
                    throw std::invalid_argument(
                        "stored table cell columns must be unique");
                }
            }
        }
    }

    void table_store::validate_groups(
        const std::vector<table_group> &groups) const {
        std::unordered_set<table_group_id> ids;
        std::size_t previous_end = 0;
        for (const auto &group_value : groups) {
            if (group_value.id == 0 ||
                !ids.insert(group_value.id).second) {
                throw std::invalid_argument(
                    "table group IDs must be non-zero and unique");
            }
            if (group_value.first_row < previous_end ||
                group_value.first_row > _rows.size() ||
                group_value.row_count >
                    _rows.size() - group_value.first_row) {
                throw std::invalid_argument(
                    "table groups must be ordered, disjoint, and bounded");
            }
            previous_end = group_value.first_row +
                           group_value.row_count;
        }
    }
} // namespace native
