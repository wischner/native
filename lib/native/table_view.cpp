//
// Implements table_view construction, cached configuration, columns,
// selection, native notifications, and lifecycle-independent state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/table_view.h>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include <native/theme.h>

#include "table_render.h"
#include "table_visible_rows.h"

namespace native
{
    table_view::table_view(coord x,
                           coord y,
                           dim width,
                           dim height)
        : wnd(x, y, width, height)
        , _visible_rows(
              std::make_unique<detail::table_visible_rows>()) {
        on_wnd_paint.connect([this](wnd_paint_event event) {
            detail::draw_table_view(*this, event.g);
            return true;
        });
        on_mouse_click.connect([this](mouse_event event) {
            return event.button == mouse_button::left &&
                   event.action == mouse_action::release &&
                   detail::handle_table_click(*this, event.position);
        });
        on_mouse_wheel.connect([this](mouse_wheel_event event) {
            if (event.direction == wheel_direction::horizontal) {
                on_native_scroll(
                    _vertical_row,
                    std::max(0, _horizontal_offset - event.delta));
            } else {
                const int direction = event.delta > 0 ? -3 : 3;
                const std::size_t next = direction < 0
                    ? _vertical_row -
                          std::min(_vertical_row,
                                   static_cast<std::size_t>(-direction))
                    : _vertical_row +
                          static_cast<std::size_t>(direction);
                on_native_scroll(next, _horizontal_offset);
            }
            return true;
        });
    }

    table_view::table_view(const point &position,
                           const size &dimensions)
        : table_view(position.x,
                     position.y,
                     dimensions.w,
                     dimensions.h) {}

    table_view::table_view(const rect &bounds)
        : table_view(bounds.p, bounds.d) {}

    table_view::~table_view() {
        destroy();
        if (_model && _model_connection)
            _model->on_change.disconnect(_model_connection);
    }

    table_view &table_view::set_model(table_model *model) {
        if (_model == model)
            return *this;
        if (_model && _model_connection)
            _model->on_change.disconnect(_model_connection);
        _model = model;
        _model_connection = 0;
        if (_model) {
            _model_connection = _model->on_change.connect(
                [this](const table_model_change &change) {
                    return handle_model_change(change);
                });
        }
        _selection.clear();
        _focused_row = invalid_table_row_id;
        _focused_model_row.reset();
        _type_search_buffer.clear();
        _type_search_deadline = 0;
        _vertical_row = 0;
        rebuild_visible_rows();
        if (_created) {
            apply_table();
            apply_selection();
            apply_scroll();
        }
        invalidate();
        return *this;
    }

    table_model *table_view::get_model() const {
        return _model;
    }

    void table_view::validate_columns(
        const std::vector<table_column> &columns) const {
        std::unordered_set<table_column_id> ids;
        for (const auto &column : columns) {
            if (column.id == 0 || !ids.insert(column.id).second)
                throw std::invalid_argument(
                    "table column IDs must be non-zero and unique");
            if (column.min_width == 0 ||
                column.min_width > column.max_width ||
                column.width < column.min_width ||
                column.width > column.max_width) {
                throw std::invalid_argument(
                    "table column width constraints are invalid");
            }
        }
    }

    table_view &table_view::set_columns(
        std::vector<table_column> columns) {
        validate_columns(columns);
        _columns = std::move(columns);
        if (_sort) {
            const auto found = std::find_if(
                _columns.begin(), _columns.end(),
                [this](const table_column &column) {
                    return column.id == _sort->column &&
                           column.sortable;
                });
            if (found == _columns.end())
                _sort.reset();
        }
        if (_created)
            apply_table();
        invalidate();
        return *this;
    }

    const std::vector<table_column> &table_view::get_columns() const {
        return _columns;
    }

    table_view &table_view::add_column(table_column column) {
        std::vector<table_column> columns = _columns;
        columns.push_back(std::move(column));
        return set_columns(std::move(columns));
    }

    table_view &table_view::remove_column(table_column_id id) {
        const auto found = std::find_if(
            _columns.begin(), _columns.end(),
            [id](const table_column &column) {
                return column.id == id;
            });
        if (found == _columns.end())
            throw std::out_of_range("table column ID is unknown");
        _columns.erase(found);
        if (_sort && _sort->column == id)
            _sort.reset();
        if (_created)
            apply_table();
        invalidate();
        return *this;
    }

    table_view &table_view::move_column(
        table_column_id id,
        std::size_t display_index) {
        if (display_index >= _columns.size())
            throw std::out_of_range("table column index is out of range");
        const auto found = std::find_if(
            _columns.begin(), _columns.end(),
            [id](const table_column &column) {
                return column.id == id;
            });
        if (found == _columns.end())
            throw std::out_of_range("table column ID is unknown");
        table_column value = std::move(*found);
        _columns.erase(found);
        _columns.insert(_columns.begin() +
                            static_cast<std::ptrdiff_t>(display_index),
                        std::move(value));
        if (_created)
            apply_table();
        invalidate();
        return *this;
    }

    table_view &table_view::set_column_width(table_column_id id,
                                             dim width) {
        const auto found = std::find_if(
            _columns.begin(), _columns.end(),
            [id](const table_column &column) {
                return column.id == id;
            });
        if (found == _columns.end())
            throw std::out_of_range("table column ID is unknown");
        found->width = std::clamp(width,
                                  found->min_width,
                                  found->max_width);
        if (_created)
            apply_table();
        invalidate();
        return *this;
    }

    table_view &table_view::set_column_visible(table_column_id id,
                                               bool visible) {
        const auto found = std::find_if(
            _columns.begin(), _columns.end(),
            [id](const table_column &column) {
                return column.id == id;
            });
        if (found == _columns.end())
            throw std::out_of_range("table column ID is unknown");
        found->visible = visible;
        if (_created)
            apply_table();
        invalidate();
        return *this;
    }

    table_view &table_view::set_header_visible(bool visible) {
        _header_visible = visible;
        if (_created)
            apply_table();
        invalidate();
        return *this;
    }

    bool table_view::get_header_visible() const {
        return _header_visible;
    }

    table_view &table_view::set_columns_reorderable(bool value) {
        _columns_reorderable = value;
        if (_created)
            apply_table();
        return *this;
    }

    bool table_view::get_columns_reorderable() const {
        return _columns_reorderable;
    }

    table_view &table_view::set_columns_resizable(bool value) {
        _columns_resizable = value;
        if (_created)
            apply_table();
        return *this;
    }

    bool table_view::get_columns_resizable() const {
        return _columns_resizable;
    }

    table_view &table_view::set_column_visibility_menu_enabled(
        bool enabled) {
        _visibility_menu = enabled;
        if (_created)
            apply_table();
        return *this;
    }

    bool table_view::get_column_visibility_menu_enabled() const {
        return _visibility_menu;
    }

    table_view &table_view::set_sort(
        std::optional<table_sort> sort) {
        if (sort) {
            const auto column = std::find_if(
                _columns.begin(), _columns.end(),
                [&sort](const table_column &candidate) {
                    return candidate.id == sort->column &&
                           candidate.sortable;
                });
            if (column == _columns.end())
                throw std::invalid_argument(
                    "table sort column is absent or not sortable");
        }
        _sort = sort;
        if (_created)
            apply_table();
        invalidate();
        return *this;
    }

    std::optional<table_sort> table_view::get_sort() const {
        return _sort;
    }

    table_view &table_view::set_data_mode(table_data_mode mode) {
        _data_mode = mode;
        if (_created)
            apply_table();
        return *this;
    }

    table_data_mode table_view::get_data_mode() const {
        return _data_mode;
    }

    table_view &table_view::set_selection_mode(
        table_selection_mode mode) {
        _selection_mode = mode;
        if (mode == table_selection_mode::single &&
            _selection.size() > 1) {
            _selection.resize(1);
            _focused_row = _selection.front();
            _focused_model_row = model_row_for_id(_focused_row);
        }
        if (_created) {
            apply_table();
            apply_selection();
        }
        invalidate();
        return *this;
    }

    table_selection_mode table_view::get_selection_mode() const {
        return _selection_mode;
    }

    std::vector<table_row_id> table_view::get_selected_rows() const {
        return _selection;
    }

    void table_view::validate_selection(
        const std::vector<table_row_id> &rows) const {
        if (_selection_mode == table_selection_mode::single &&
            rows.size() > 1) {
            throw std::invalid_argument(
                "single-selection table accepts at most one row");
        }
        std::unordered_set<table_row_id> ids;
        for (table_row_id id : rows) {
            if (id == invalid_table_row_id ||
                !ids.insert(id).second || !model_row_for_id(id)) {
                throw std::invalid_argument(
                    "table selection contains an unknown row ID");
            }
        }
    }

    table_view &table_view::set_selected_rows(
        const std::vector<table_row_id> &rows) {
        validate_selection(rows);
        _selection = rows;
        std::sort(_selection.begin(), _selection.end(),
                  [this](table_row_id left, table_row_id right) {
                      return *model_row_for_id(left) <
                             *model_row_for_id(right);
                  });
        _focused_row = _selection.empty()
                           ? invalid_table_row_id
                           : _selection.back();
        _focused_model_row = _selection.empty()
                                 ? std::nullopt
                                 : model_row_for_id(_focused_row);
        if (_created)
            apply_selection();
        invalidate();
        return *this;
    }

    table_view &table_view::set_alternating_rows(bool enabled) {
        _alternating_rows = enabled;
        if (_created)
            apply_table();
        invalidate();
        return *this;
    }

    bool table_view::get_alternating_rows() const {
        return _alternating_rows;
    }

    table_view &table_view::set_grid_lines(table_grid_lines lines) {
        _grid_lines = lines;
        if (_created)
            apply_table();
        invalidate();
        return *this;
    }

    table_grid_lines table_view::get_grid_lines() const {
        return _grid_lines;
    }

    table_view &table_view::set_row_height(
        std::optional<dim> height) {
        if (height && *height == 0)
            throw std::invalid_argument(
                "table row height must be non-zero");
        _row_height = height;
        if (_created)
            apply_table();
        invalidate();
        return *this;
    }

    std::optional<dim> table_view::get_row_height() const {
        return _row_height;
    }

    table_view &table_view::set_icon_size(
        std::optional<size> dimensions) {
        if (dimensions && (!dimensions->w || !dimensions->h))
            throw std::invalid_argument(
                "table icon dimensions must be non-zero");
        _icon_size = dimensions;
        if (_created)
            apply_table();
        invalidate();
        return *this;
    }

    std::optional<size> table_view::get_icon_size() const {
        return _icon_size;
    }

    table_view &table_view::set_vertical_scrollbar_policy(
        scrollbar_policy policy) {
        _vertical_policy = policy;
        if (_created)
            apply_table();
        invalidate();
        return *this;
    }

    scrollbar_policy
    table_view::get_vertical_scrollbar_policy() const {
        return _vertical_policy;
    }

    table_view &table_view::set_horizontal_scrollbar_policy(
        scrollbar_policy policy) {
        _horizontal_policy = policy;
        if (_created)
            apply_table();
        invalidate();
        return *this;
    }

    scrollbar_policy
    table_view::get_horizontal_scrollbar_policy() const {
        return _horizontal_policy;
    }

    table_view &table_view::set_type_search_enabled(bool enabled) {
        _type_search = enabled;
        if (!enabled) {
            _type_search_buffer.clear();
            _type_search_deadline = 0;
        }
        if (_created)
            apply_table();
        return *this;
    }

    bool table_view::get_type_search_enabled() const {
        return _type_search;
    }

    bool table_view::get_focused() const {
        return _focused;
    }

    void table_view::on_native_focus(bool focused) {
        if (_focused == focused)
            return;
        _focused = focused;
        invalidate();
    }

    void table_view::on_bounds_changed() {
        const std::size_t maximum = get_display_row_count() > 0
            ? get_display_row_count() - 1
            : 0;
        _vertical_row = std::min(_vertical_row, maximum);
        if (_created)
            apply_scroll();
        invalidate();
    }

    void table_view::synchronize_theme_metrics() {
        try {
            auto painter = theme::create(get_gpx());
            const theme::metrics values = painter->defaults();
            _native_row_height = std::max(1, values.list_item_height);
            _native_header_height = std::max(1, values.header_height);
        } catch (const std::runtime_error &) {
            // Some Xt and WINGs parents finish realization after child
            // construction; their stock defaults remain usable here.
        }
    }
} // namespace native
