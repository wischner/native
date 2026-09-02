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
    namespace
    {
        rect fit_table_image(const img &image, rect box) {
            if (!box.d.w || !box.d.h)
                return rect(box.p, size());
            const double scale = std::min(
                {1.0,
                 static_cast<double>(box.d.w) / image.w(),
                 static_cast<double>(box.d.h) / image.h()});
            const int width = std::max(
                1, static_cast<int>(image.w() * scale));
            const int height = std::max(
                1, static_cast<int>(image.h() * scale));
            return rect(
                static_cast<coord>(
                    box.p.x +
                    (static_cast<int>(box.d.w) - width) / 2),
                static_cast<coord>(
                    box.p.y +
                    (static_cast<int>(box.d.h) - height) / 2),
                static_cast<dim>(width),
                static_cast<dim>(height));
        }
    } // namespace

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
            _native_row_height = std::max(1, values.table_row_height);
            _native_header_height = std::max(1, values.header_height);
        } catch (const std::runtime_error &) {
            // Some Xt and WINGs parents finish realization after child
            // construction; their stock defaults remain usable here.
        }
    }

    void table_view::draw_background(
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        if (appearance.defaults().table_outer_border_extent > 0) {
            graphics.set_ink(appearance.native_palette().content_bg)
                .draw_rect(bounds, true);
            return;
        }
        appearance.draw_surface(bounds, surface_kind::inset, state);
    }

    void table_view::draw_border(
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &) {
        const int extent = std::max(
            0, appearance.defaults().table_outer_border_extent);
        const theme::palette colors = appearance.native_palette();
        graphics.set_pen(1);
        for (int inset = 0; inset < extent; ++inset) {
            const int width = static_cast<int>(bounds.d.w) - inset * 2;
            const int height = static_cast<int>(bounds.d.h) - inset * 2;
            if (width <= 0 || height <= 0)
                break;
            const coord left = static_cast<coord>(bounds.p.x + inset);
            const coord top = static_cast<coord>(bounds.p.y + inset);
            const coord right = static_cast<coord>(left + width - 1);
            const coord bottom = static_cast<coord>(top + height - 1);
            graphics.set_ink(colors.button_border)
                .draw_line(point(left, top), point(right, top))
                .draw_line(point(left, top), point(left, bottom));
            graphics.set_ink(colors.button_highlight)
                .draw_line(point(left, bottom), point(right, bottom))
                .draw_line(point(right, top), point(right, bottom));
        }
    }

    void table_view::draw_header_background(
        gpx &,
        theme &appearance,
        const table_column &,
        const rect &bounds,
        const theme::state &state) {
        const int frame = std::max(
            0, appearance.defaults().table_outer_border_extent);
        const int left = std::max<int>(bounds.p.x, frame);
        const int top = std::max<int>(bounds.p.y, frame);
        const int right = bounds.x2();
        const int bottom = bounds.y2();
        if (right <= left || bottom <= top)
            return;
        // Keep the header surface inside the table's inset viewport edge.
        // The distinct semantic kind lets a backend match native column
        // headers without changing accordion, collection, or docking
        // headers which use the ordinary header role.
        appearance.draw_surface(
            rect(static_cast<coord>(left),
                 static_cast<coord>(top),
                 static_cast<dim>(right - left),
                 static_cast<dim>(bottom - top)),
            surface_kind::table_header,
            state);
    }

    void table_view::draw_header_content(
        gpx &graphics,
        theme &appearance,
        const table_column &column,
        const rect &bounds,
        const theme::state &state) {
        const theme::metrics metrics = appearance.defaults();
        const theme::palette colors = appearance.native_palette();
        const bool sorted = _sort && _sort->column == column.id;
        const int indicator_width = sorted
                                        ? std::min<int>(
                                              12, bounds.d.w)
                                        : 0;
        graphics.set_font(font_t::stock(font_role::control))
            .set_ink(state.disabled ? colors.button_disabled_text
                                    : colors.button_text)
            .draw_text(
                column.title,
                rect(static_cast<coord>(
                         bounds.p.x + metrics.header_padding_x),
                     bounds.p.y,
                     static_cast<dim>(std::max(
                         0,
                         static_cast<int>(bounds.d.w) -
                             metrics.header_padding_x * 2 -
                             indicator_width)),
                     bounds.d.h),
                text_layout{text_align::start,
                            text_valign::center,
                            text_overflow::ellipsis,
                            true});
        if (sorted) {
            const int side = std::max(
                3, std::min(8, static_cast<int>(bounds.d.h) - 6));
            appearance.draw_sort_indicator(
                rect(static_cast<coord>(bounds.x2() - side - 4),
                     static_cast<coord>(
                         bounds.p.y +
                         (static_cast<int>(bounds.d.h) - side) / 2),
                     static_cast<dim>(side),
                     static_cast<dim>(side)),
                _sort->direction == sort_direction::ascending
                    ? sort_indicator_state::ascending
                    : sort_indicator_state::descending,
                state);
        }
    }

    void table_view::draw_header_border(
        gpx &,
        theme &appearance,
        const table_column &,
        const rect &bounds,
        const theme::state &) {
        appearance.draw_separator(
            rect(static_cast<coord>(bounds.x2() - 1),
                 bounds.p.y,
                 1,
                 bounds.d.h),
            separator_orientation::vertical);
    }

    void table_view::draw_group(
        gpx &graphics,
        theme &appearance,
        const table_group &group,
        const rect &bounds,
        const theme::state &state) {
        const theme::metrics metrics = appearance.defaults();
        const theme::palette colors = appearance.native_palette();
        appearance.draw_surface(bounds, surface_kind::header, state);
        int text_x = bounds.p.x + metrics.header_padding_x;
        if (group.collapsible) {
            const int side = std::min(
                metrics.disclosure_size,
                std::max(1, static_cast<int>(bounds.d.h) - 4));
            const rect disclosure(
                static_cast<coord>(text_x),
                static_cast<coord>(
                    bounds.p.y +
                    (static_cast<int>(bounds.d.h) - side) / 2),
                static_cast<dim>(side),
                static_cast<dim>(side));
            appearance.draw_disclosure(
                disclosure,
                get_group_expanded(group.id)
                    ? disclosure_state::expanded
                    : disclosure_state::collapsed,
                state);
            text_x = disclosure.x2() + metrics.header_gap;
        }
        graphics.set_font(font_t::stock(font_role::control))
            .set_ink(state.disabled ? colors.button_disabled_text
                                    : colors.button_text)
            .draw_text(
                group.title,
                rect(static_cast<coord>(text_x),
                     bounds.p.y,
                     static_cast<dim>(std::max(
                         0, bounds.x2() - text_x -
                                metrics.header_padding_x)),
                     bounds.d.h),
                text_layout{text_align::start,
                            text_valign::center,
                            text_overflow::ellipsis,
                            true});
    }

    void table_view::draw_row_background(
        gpx &graphics,
        theme &appearance,
        table_row_id,
        std::size_t model_row,
        const rect &bounds,
        const theme::state &state) {
        const theme::palette colors = appearance.native_palette();
        rgba background = colors.content_bg;
        if (_alternating_rows && (model_row & 1U) != 0) {
            if (colors.content_alt_bg.a != 0) {
                background = colors.content_alt_bg;
            } else {
                background = rgba(
                    static_cast<std::uint8_t>(
                        (static_cast<unsigned int>(colors.content_bg.r) *
                             247 +
                         static_cast<unsigned int>(colors.separator.r) *
                             8) /
                        255),
                    static_cast<std::uint8_t>(
                        (static_cast<unsigned int>(colors.content_bg.g) *
                             247 +
                         static_cast<unsigned int>(colors.separator.g) *
                             8) /
                        255),
                    static_cast<std::uint8_t>(
                        (static_cast<unsigned int>(colors.content_bg.b) *
                             247 +
                         static_cast<unsigned int>(colors.separator.b) *
                             8) /
                        255),
                    255);
            }
        }
        graphics.set_ink(background).draw_rect(bounds, true);
        appearance.draw_selection(bounds, selection_shape::row, state);
    }

    void table_view::draw_cell_background(
        gpx &,
        theme &,
        table_row_id,
        std::size_t,
        const table_column &,
        const table_cell &,
        const rect &,
        const theme::state &) {}

    void table_view::draw_cell_content(
        gpx &graphics,
        theme &appearance,
        table_row_id,
        std::size_t,
        const table_column &column,
        const table_cell &cell,
        const rect &bounds,
        const theme::state &state) {
        const theme::metrics metrics = appearance.defaults();
        const theme::palette colors = appearance.native_palette();
        int text_x = bounds.p.x + metrics.header_padding_x;
        int available = static_cast<int>(bounds.d.w) -
                        metrics.header_padding_x * 2;
        if (cell.image && column.allow_image) {
            const size requested = _icon_size.value_or(size(
                static_cast<dim>(std::max(
                    1, static_cast<int>(bounds.d.h) - 6)),
                static_cast<dim>(std::max(
                    1, static_cast<int>(bounds.d.h) - 6))));
            const int image_width = std::min<int>(
                requested.w, std::max(1, available));
            const int image_height = std::min<int>(
                requested.h,
                std::max(1, static_cast<int>(bounds.d.h) - 4));
            const rect image_box(
                static_cast<coord>(text_x),
                static_cast<coord>(
                    bounds.p.y +
                    (static_cast<int>(bounds.d.h) - image_height) / 2),
                static_cast<dim>(image_width),
                static_cast<dim>(image_height));
            graphics.draw_img(*cell.image,
                              fit_table_image(*cell.image, image_box),
                              image_filter::linear);
            text_x = image_box.x2() + metrics.header_gap;
            available -= image_width + metrics.header_gap;
        }
        graphics.set_font(font_t::stock(font_role::control));
        const text_metrics measured = graphics.measure_text(cell.text);
        if (!cell.image || column.alignment != table_alignment::start) {
            if (column.alignment == table_alignment::center) {
                text_x = bounds.p.x +
                         (static_cast<int>(bounds.d.w) -
                          measured.width) /
                             2;
            } else if (column.alignment == table_alignment::end) {
                text_x = bounds.x2() - measured.width -
                         metrics.header_padding_x;
            }
        }
        graphics.set_ink(state.selected ? colors.selection_text
                                        : colors.content_text)
            .draw_text(
                cell.text,
                rect(static_cast<coord>(text_x),
                     bounds.p.y,
                     static_cast<dim>(std::max(0, available)),
                     bounds.d.h),
                text_layout{text_align::start,
                            text_valign::center,
                            text_overflow::ellipsis,
                            true});
    }

    void table_view::draw_cell_border(
        gpx &,
        theme &appearance,
        table_row_id,
        std::size_t,
        const table_column &,
        const table_cell &,
        const rect &bounds,
        const theme::state &) {
        const auto lines = static_cast<std::uint8_t>(_grid_lines);
        if ((lines & static_cast<std::uint8_t>(
                         table_grid_lines::vertical)) != 0) {
            appearance.draw_separator(
                rect(static_cast<coord>(bounds.x2() - 1),
                     bounds.p.y,
                     1,
                     bounds.d.h),
                separator_orientation::vertical);
        }
        if ((lines & static_cast<std::uint8_t>(
                         table_grid_lines::horizontal)) != 0) {
            appearance.draw_separator(
                rect(bounds.p.x,
                     static_cast<coord>(bounds.y2() - 1),
                     bounds.d.w,
                     1),
                separator_orientation::horizontal);
        }
    }

    void table_view::draw_row_focus(
        gpx &,
        theme &appearance,
        table_row_id,
        std::size_t,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_focus(bounds, state);
    }

    void table_view::draw_scrollbar(
        gpx &,
        theme &appearance,
        scrollbar_orientation orientation,
        const rect &track,
        const rect &thumb,
        const theme::state &state) {
        appearance.draw_scrollbar_part(
            track, orientation, scrollbar_part::track, state);
        appearance.draw_scrollbar_part(
            thumb, orientation, scrollbar_part::thumb, state);
    }
} // namespace native
