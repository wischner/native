//
// Implements table_view model invalidation, grouped row mapping,
// searching, scrolling, and native-originated action translation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/table_view.h>

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <unordered_set>

#include "table_visible_rows.h"

namespace native
{
    void table_view::rebuild_visible_rows() {
        _visible_rows->reset(_model);
        const std::size_t count = get_display_row_count();
        _vertical_row = count == 0
                            ? 0
                            : std::min(_vertical_row, count - 1);
    }

    std::optional<std::size_t> table_view::model_row_for_id(
        table_row_id id) const {
        if (!_model || id == invalid_table_row_id)
            return std::nullopt;
        for (std::size_t row = 0; row < _model->row_count(); ++row) {
            if (_model->row_id(row) == id)
                return row;
        }
        return std::nullopt;
    }

    std::optional<std::size_t> table_view::display_row_for_id(
        table_row_id id) const {
        const auto model_row = model_row_for_id(id);
        return model_row
                   ? _visible_rows->display_index_for_model_row(
                         *model_row)
                   : std::nullopt;
    }

    bool table_view::handle_model_change(
        const table_model_change &change) {
        // A reset is emitted after the model has replaced its contents, while
        // the visible-row map still describes the previous contents.  There
        // is therefore no safe stable row ID to preserve across a reset.
        // Incremental changes can retain the best available top-row anchor.
        const auto top = change.kind == table_model_change_kind::reset
                             ? std::nullopt
                             : get_first_visible_row();
        rebuild_visible_rows();
        _selection.erase(
            std::remove_if(_selection.begin(), _selection.end(),
                           [this](table_row_id id) {
                               return !model_row_for_id(id);
                           }),
            _selection.end());
        if (_focused_row != invalid_table_row_id &&
            !model_row_for_id(_focused_row)) {
            _focused_row = _selection.empty()
                               ? invalid_table_row_id
                               : _selection.back();
        }
        _focused_model_row = _focused_row == invalid_table_row_id
                                 ? std::nullopt
                                 : model_row_for_id(_focused_row);
        if (top) {
            const auto display = display_row_for_id(*top);
            if (display)
                _vertical_row = *display;
        }
        if (_created) {
            apply_table();
            apply_selection();
            apply_scroll();
        }
        invalidate();
        return false;
    }

    table_view &table_view::set_group_expanded(table_group_id id,
                                               bool expanded) {
        if (_visible_rows->get_expanded(id) == expanded)
            return *this;
        const std::size_t old_count = get_display_row_count();
        const std::optional<table_display_row> top_display =
            old_count == 0
                ? std::nullopt
                : std::optional<table_display_row>(get_display_row(
                      std::min(_vertical_row, old_count - 1)));
        const auto top = get_first_visible_row();
        _visible_rows->set_expanded(id, expanded);
        if (top_display && top_display->group) {
            const auto display =
                _visible_rows->display_index_for_group(
                    top_display->group_id);
            if (display)
                _vertical_row = *display;
        } else if (top) {
            const auto display = display_row_for_id(*top);
            if (display)
                _vertical_row = *display;
        }
        const std::size_t count = get_display_row_count();
        _vertical_row = count == 0
                            ? 0
                            : std::min(_vertical_row, count - 1);
        if (_created) {
            apply_table();
            apply_selection();
            apply_scroll();
        }
        invalidate();
        return *this;
    }

    bool table_view::get_group_expanded(table_group_id id) const {
        return _visible_rows->get_expanded(id);
    }

    std::optional<std::size_t>
    table_view::find_model_row(const table_search &query) const {
        if (!_model || _model->row_count() == 0)
            return std::nullopt;
        table_search normalized = query;
        if (normalized.columns.empty()) {
            for (const auto &column : _columns) {
                if (column.visible)
                    normalized.columns.push_back(column.id);
            }
        }
        if (normalized.columns.empty())
            return std::nullopt;
        for (table_column_id id : normalized.columns) {
            const auto found = std::find_if(
                _columns.begin(), _columns.end(),
                [id](const table_column &column) {
                    return column.id == id;
                });
            if (found == _columns.end())
                throw std::invalid_argument(
                    "table search column ID is unknown");
        }
        const auto row = _model->find(normalized);
        if (!row)
            return std::nullopt;
        if (*row >= _model->row_count())
            throw std::runtime_error(
                "table model find returned an invalid row");
        return row;
    }

    table_row_id table_view::row_id_for_model_row(
        std::size_t model_row) const {
        const table_row_id id = _model->row_id(model_row);
        if (id == invalid_table_row_id)
            throw std::runtime_error(
                "table model returned the reserved row ID");
        return id;
    }

    std::optional<table_row_id>
    table_view::find(const table_search &query) const {
        const auto row = find_model_row(query);
        return row ? std::optional<table_row_id>(
                         row_id_for_model_row(*row))
                   : std::nullopt;
    }

    std::optional<table_row_id>
    table_view::find_text(const std::string &text,
                          table_search_match match) const {
        table_search query;
        query.text = text;
        query.match = match;
        return find(query);
    }

    bool table_view::find_and_select(const table_search &query) {
        const auto row = find_model_row(query);
        if (!row)
            return false;
        select_known_row(*row, false);
        return true;
    }

    bool table_view::find_and_reveal(const table_search &query) {
        const auto row = find_model_row(query);
        if (!row)
            return false;
        const auto group = _visible_rows->group_for_model_row(*row);
        if (group && !_visible_rows->get_expanded(group->id)) {
            _visible_rows->set_expanded(group->id, true);
            if (_created)
                apply_table();
            invalidate();
        }
        select_known_row(*row, false);
        ensure_model_row_visible(*row);
        return true;
    }

    void table_view::select_known_row(std::size_t model_row,
                                      bool emit) {
        const table_row_id id = row_id_for_model_row(model_row);
        const std::vector<table_row_id> selected{id};
        if (_selection == selected) {
            _focused_model_row = model_row;
            if (_created)
                apply_selection();
            return;
        }
        _selection = selected;
        _focused_row = id;
        _focused_model_row = model_row;
        if (_created)
            apply_selection();
        invalidate();
        if (emit)
            on_selection_change.emit(_selection);
    }

    std::size_t table_view::rows_per_page() const {
        const int header = _header_visible ? _native_header_height : 0;
        int available = std::max(
            1, static_cast<int>(_bounds.d.h) - header);
        const int height = _row_height
                               ? static_cast<int>(*_row_height)
                               : _native_row_height;
        int content_width = 0;
        for (const table_column &column : _columns) {
            if (column.visible)
                content_width += column.width;
        }
        const bool horizontal =
            _horizontal_policy == scrollbar_policy::always ||
            (_horizontal_policy == scrollbar_policy::automatic &&
             content_width > static_cast<int>(_bounds.d.w));
        if (horizontal)
            available = std::max(
                1, available - std::max(1, _theme_metrics.scrollbar_extent));
        return static_cast<std::size_t>(
            std::max(1, (available + std::max(1, height) - 1) /
                            std::max(1, height)));
    }

    table_view &table_view::scroll_to_row(table_row_id id) {
        const auto display = display_row_for_id(id);
        if (!display)
            return *this;
        _vertical_row = *display;
        if (_created)
            apply_scroll();
        invalidate();
        return *this;
    }

    table_view &table_view::ensure_row_visible(table_row_id id) {
        const auto model_row = model_row_for_id(id);
        if (model_row)
            ensure_model_row_visible(*model_row);
        return *this;
    }

    void table_view::ensure_model_row_visible(
        std::size_t model_row) {
        const auto display =
            _visible_rows->display_index_for_model_row(model_row);
        if (!display)
            return;
        const std::size_t page = rows_per_page();
        if (*display < _vertical_row)
            _vertical_row = *display;
        else if (*display >= _vertical_row + page)
            _vertical_row = *display - page + 1;
        if (_created)
            apply_scroll();
        invalidate();
    }

    table_visible_range table_view::get_visible_row_range() const {
        const std::size_t count = get_display_row_count();
        if (count == 0)
            return {};
        const std::size_t first = std::min(_vertical_row, count - 1);
        return {first, std::min(rows_per_page(), count - first)};
    }

    std::optional<table_row_id>
    table_view::get_first_visible_row() const {
        const table_visible_range range = get_visible_row_range();
        for (std::size_t offset = 0; offset < range.count; ++offset) {
            const table_display_row row =
                get_display_row(range.first + offset);
            if (!row.group && _model &&
                row.model_row < _model->row_count()) {
                return _model->row_id(row.model_row);
            }
        }
        return std::nullopt;
    }

    std::optional<table_row_id>
    table_view::get_last_visible_row() const {
        const table_visible_range range = get_visible_row_range();
        for (std::size_t offset = range.count; offset > 0; --offset) {
            const table_display_row row =
                get_display_row(range.first + offset - 1);
            if (!row.group && _model &&
                row.model_row < _model->row_count()) {
                return _model->row_id(row.model_row);
            }
        }
        return std::nullopt;
    }

    std::size_t table_view::get_display_row_count() const {
        return _visible_rows->display_row_count();
    }

    table_display_row table_view::get_display_row(
        std::size_t display_index) const {
        return _visible_rows->row_at(display_index);
    }

    int table_view::get_horizontal_scroll_offset() const {
        return _horizontal_offset;
    }

    std::size_t table_view::get_vertical_scroll_row() const {
        return _vertical_row;
    }

    void table_view::on_native_selection(
        const std::vector<table_row_id> &rows) {
        validate_selection(rows);
        std::vector<table_row_id> normalized = rows;
        std::sort(normalized.begin(), normalized.end(),
                  [this](table_row_id left, table_row_id right) {
                      return *model_row_for_id(left) <
                             *model_row_for_id(right);
                  });
        if (_selection == normalized)
            return;
        _selection = std::move(normalized);
        _focused_row = _selection.empty()
                           ? invalid_table_row_id
                           : _selection.back();
        _focused_model_row = _selection.empty()
                                 ? std::nullopt
                                 : model_row_for_id(_focused_row);
        if (_created)
            apply_selection();
        invalidate();
        on_selection_change.emit(_selection);
    }

    void table_view::on_native_activate(table_row_id id) {
        if (model_row_for_id(id))
            on_row_activate.emit(id);
    }

    void table_view::on_native_type_text(const std::string &text) {
        if (!_type_search || !_model || text.empty())
            return;
        const auto now = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());
        if (now > _type_search_deadline)
            _type_search_buffer.clear();
        _type_search_buffer += text;
        _type_search_deadline = now + 1000;

        table_search query;
        query.text = _type_search_buffer;
        query.match = table_search_match::prefix;
        const auto primary = std::find_if(
            _columns.begin(), _columns.end(),
            [](const table_column &column) {
                return column.visible;
            });
        if (primary == _columns.end())
            return;
        query.columns = {primary->id};
        std::optional<std::size_t> row;
        try {
            row = find_model_row(query);
            if (!row && _type_search_buffer != text) {
                _type_search_buffer = text;
                query.text = text;
                row = find_model_row(query);
            }
        } catch (const std::invalid_argument &) {
            _type_search_buffer.clear();
            return;
        }
        if (!row)
            return;
        const auto group = _visible_rows->group_for_model_row(*row);
        if (group && !_visible_rows->get_expanded(group->id)) {
            _visible_rows->set_expanded(group->id, true);
            if (_created)
                apply_table();
            invalidate();
        }
        select_known_row(*row, true);
        ensure_model_row_visible(*row);
    }

    void table_view::on_native_sort_request(table_column_id id) {
        const auto column = std::find_if(
            _columns.begin(), _columns.end(),
            [id](const table_column &candidate) {
                return candidate.id == id && candidate.sortable;
            });
        if (column == _columns.end())
            return;
        table_sort request;
        request.column = id;
        request.direction =
            _sort && _sort->column == id &&
                    _sort->direction == sort_direction::ascending
                ? sort_direction::descending
                : sort_direction::ascending;
        _sort = request;
        if (_created)
            apply_table();
        invalidate();
        on_sort_request.emit(request);
    }

    void table_view::on_native_column_resize(table_column_id id,
                                             dim width) {
        const auto column = std::find_if(
            _columns.begin(), _columns.end(),
            [id](const table_column &candidate) {
                return candidate.id == id;
            });
        if (column == _columns.end())
            return;
        const dim clamped = std::clamp(width,
                                       column->min_width,
                                       column->max_width);
        if (column->width == clamped)
            return;
        column->width = clamped;
        invalidate();
        on_column_resize.emit(id, clamped);
    }

    void table_view::on_native_column_move(
        table_column_id id,
        std::size_t display_index) {
        if (!_columns_reorderable || display_index >= _columns.size())
            return;
        const auto found = std::find_if(
            _columns.begin(), _columns.end(),
            [id](const table_column &column) {
                return column.id == id;
            });
        if (found == _columns.end())
            return;
        const std::size_t old = static_cast<std::size_t>(
            found - _columns.begin());
        if (old == display_index)
            return;
        table_column value = std::move(*found);
        _columns.erase(found);
        _columns.insert(_columns.begin() +
                            static_cast<std::ptrdiff_t>(display_index),
                        std::move(value));
        invalidate();
        on_column_move.emit(id, display_index);
    }

    void table_view::on_native_group_expand(table_group_id id,
                                            bool expanded) {
        const auto group = std::find_if(
            _visible_rows->groups().begin(),
            _visible_rows->groups().end(),
            [id](const table_group &candidate) {
                return candidate.id == id;
            });
        if (group == _visible_rows->groups().end() ||
            !group->collapsible ||
            get_group_expanded(id) == expanded) {
            return;
        }
        set_group_expanded(id, expanded);
        on_group_expand.emit(id, expanded);
    }

    void table_view::on_native_scroll(
        std::size_t first_display_row,
        int horizontal_offset) {
        const std::size_t count = get_display_row_count();
        const std::size_t maximum = count > rows_per_page()
                                        ? count - rows_per_page()
                                        : 0;
        const std::size_t vertical =
            std::min(first_display_row, maximum);
        int content_width = 0;
        for (const auto &column : _columns) {
            if (column.visible)
                content_width += column.width;
        }
        const int horizontal_maximum = std::max(
            0, content_width - static_cast<int>(_bounds.d.w));
        const int horizontal = std::clamp(
            horizontal_offset, 0, horizontal_maximum);
        if (_vertical_row == vertical &&
            _horizontal_offset == horizontal) {
            return;
        }
        _vertical_row = vertical;
        _horizontal_offset = horizontal;
        if (_created)
            apply_scroll();
        invalidate();
    }

    void table_view::on_native_navigation(
        table_navigation navigation,
        bool extend) {
        if (!_model || get_display_row_count() == 0)
            return;
        if (navigation == table_navigation::select_all) {
            if (_selection_mode != table_selection_mode::multiple)
                return;
            std::vector<table_row_id> rows;
            rows.reserve(_model->row_count());
            for (std::size_t row = 0;
                 row < _model->row_count(); ++row) {
                rows.push_back(_model->row_id(row));
            }
            on_native_selection(rows);
            return;
        }
        if (navigation == table_navigation::activate) {
            if (_focused_row != invalid_table_row_id)
                on_native_activate(_focused_row);
            return;
        }
        const auto focused_display =
            _focused_model_row
                ? _visible_rows->display_index_for_model_row(
                      *_focused_model_row)
                : std::nullopt;
        std::size_t candidate = focused_display.value_or(_vertical_row);
        switch (navigation) {
        case table_navigation::up:
            candidate -= std::min(candidate, std::size_t{1});
            break;
        case table_navigation::down:
            ++candidate;
            break;
        case table_navigation::home:
            candidate = 0;
            break;
        case table_navigation::end:
            candidate = get_display_row_count() - 1;
            break;
        case table_navigation::page_up:
            candidate -= std::min(candidate, rows_per_page());
            break;
        case table_navigation::page_down:
            candidate += rows_per_page();
            break;
        case table_navigation::collapse:
        case table_navigation::expand: {
            const auto group = _focused_model_row
                                   ? _visible_rows->group_for_model_row(
                                         *_focused_model_row)
                                   : std::nullopt;
            if (group && group->collapsible) {
                on_native_group_expand(
                    group->id,
                    navigation == table_navigation::expand);
            }
            return;
        }
        case table_navigation::toggle:
        case table_navigation::activate:
        case table_navigation::select_all:
            break;
        }
        candidate = std::min(candidate, get_display_row_count() - 1);
        while (get_display_row(candidate).group) {
            if (navigation == table_navigation::up ||
                navigation == table_navigation::page_up ||
                navigation == table_navigation::end) {
                if (candidate == 0)
                    return;
                --candidate;
            } else {
                if (++candidate >= get_display_row_count())
                    return;
            }
        }
        const table_row_id id = _model->row_id(
            get_display_row(candidate).model_row);
        std::vector<table_row_id> selection;
        if (extend &&
            _selection_mode == table_selection_mode::multiple) {
            selection = _selection;
            if (std::find(selection.begin(), selection.end(), id) ==
                selection.end()) {
                selection.push_back(id);
            }
        } else if (navigation == table_navigation::toggle &&
                   _selection_mode == table_selection_mode::multiple) {
            selection = _selection;
            const auto selected = std::find(selection.begin(),
                                             selection.end(), id);
            if (selected == selection.end())
                selection.push_back(id);
            else
                selection.erase(selected);
        } else {
            selection = {id};
        }
        on_native_selection(selection);
        _focused_row = id;
        _focused_model_row = get_display_row(candidate).model_row;
        ensure_model_row_visible(*_focused_model_row);
    }
} // namespace native
