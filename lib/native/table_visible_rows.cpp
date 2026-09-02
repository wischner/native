//
// Implements compact grouped display-row calculations. All mapping
// costs are proportional to group count rather than logical row count.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "table_visible_rows.h"

#include <stdexcept>
#include <unordered_set>

namespace native::detail
{
    table_visible_rows::table_visible_rows() = default;

    void table_visible_rows::reset(table_model *model) {
        _model = model;
        _row_count = model ? model->row_count() : 0;
        std::vector<table_group> groups;
        std::unordered_set<table_group_id> ids;
        std::size_t previous_end = 0;
        if (model) {
            groups.reserve(model->group_count());
            for (std::size_t index = 0;
                 index < model->group_count(); ++index) {
                table_group value = model->group(index);
                if (value.id == 0 || !ids.insert(value.id).second ||
                    value.first_row < previous_end ||
                    value.first_row > _row_count ||
                    value.row_count > _row_count - value.first_row) {
                    throw std::invalid_argument(
                        "table groups must have unique IDs and "
                        "ordered bounded ranges");
                }
                previous_end = value.first_row + value.row_count;
                groups.push_back(std::move(value));
            }
        }
        std::unordered_map<table_group_id, bool> expansion;
        for (const auto &group : groups) {
            const auto found = _expanded.find(group.id);
            expansion[group.id] = found == _expanded.end()
                                      ? group.expanded
                                      : found->second;
        }
        _groups = std::move(groups);
        _expanded = std::move(expansion);
    }

    bool table_visible_rows::expanded(
        const table_group &group) const {
        const auto found = _expanded.find(group.id);
        return found == _expanded.end() ? group.expanded
                                        : found->second;
    }

    std::size_t table_visible_rows::display_row_count() const {
        std::size_t result = _row_count + _groups.size();
        for (const auto &group : _groups) {
            if (!expanded(group))
                result -= group.row_count;
        }
        return result;
    }

    table_display_row table_visible_rows::row_at(
        std::size_t display_index) const {
        if (display_index >= display_row_count())
            throw std::out_of_range("table display row is out of range");
        std::size_t model_cursor = 0;
        std::size_t display_cursor = 0;
        for (const auto &group : _groups) {
            const std::size_t gap = group.first_row - model_cursor;
            if (display_index < display_cursor + gap) {
                return {false,
                        0,
                        model_cursor + display_index - display_cursor};
            }
            display_cursor += gap;
            if (display_index == display_cursor)
                return {true, group.id, group.first_row};
            ++display_cursor;
            if (expanded(group)) {
                if (display_index < display_cursor + group.row_count) {
                    return {false,
                            group.id,
                            group.first_row + display_index -
                                display_cursor};
                }
                display_cursor += group.row_count;
            }
            model_cursor = group.first_row + group.row_count;
        }
        return {false,
                0,
                model_cursor + display_index - display_cursor};
    }

    std::optional<std::size_t>
    table_visible_rows::display_index_for_model_row(
        std::size_t model_row) const {
        if (model_row >= _row_count)
            return std::nullopt;
        std::size_t model_cursor = 0;
        std::size_t display_cursor = 0;
        for (const auto &group : _groups) {
            if (model_row < group.first_row) {
                return display_cursor + model_row - model_cursor;
            }
            display_cursor += group.first_row - model_cursor;
            ++display_cursor;
            if (model_row < group.first_row + group.row_count) {
                if (!expanded(group))
                    return std::nullopt;
                return display_cursor + model_row - group.first_row;
            }
            if (expanded(group))
                display_cursor += group.row_count;
            model_cursor = group.first_row + group.row_count;
        }
        return display_cursor + model_row - model_cursor;
    }

    std::optional<std::size_t>
    table_visible_rows::display_index_for_group(
        table_group_id id) const {
        std::size_t model_cursor = 0;
        std::size_t display_cursor = 0;
        for (const auto &group : _groups) {
            display_cursor += group.first_row - model_cursor;
            if (group.id == id)
                return display_cursor;
            ++display_cursor;
            if (expanded(group))
                display_cursor += group.row_count;
            model_cursor = group.first_row + group.row_count;
        }
        return std::nullopt;
    }

    std::optional<std::size_t>
    table_visible_rows::model_row_for_display_index(
        std::size_t display_index) const {
        if (display_index >= display_row_count())
            return std::nullopt;
        const table_display_row mapped = row_at(display_index);
        return mapped.group ? std::nullopt
                            : std::optional<std::size_t>(mapped.model_row);
    }

    std::optional<table_group>
    table_visible_rows::group_for_model_row(
        std::size_t model_row) const {
        for (const auto &group : _groups) {
            if (model_row >= group.first_row &&
                model_row < group.first_row + group.row_count) {
                return group;
            }
        }
        return std::nullopt;
    }

    void table_visible_rows::set_expanded(table_group_id id,
                                          bool is_expanded) {
        if (_expanded.find(id) == _expanded.end())
            throw std::out_of_range("table group ID is unknown");
        _expanded[id] = is_expanded;
    }

    bool table_visible_rows::get_expanded(table_group_id id) const {
        const auto found = _expanded.find(id);
        if (found == _expanded.end())
            throw std::out_of_range("table group ID is unknown");
        return found->second;
    }

    const std::vector<table_group> &table_visible_rows::groups() const {
        return _groups;
    }
} // namespace native::detail
