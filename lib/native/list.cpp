//
// Implements backend-neutral list items and selection state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>
#include <utility>

#include <native.h>
#include <native/list.h>
#include <native/theme.h>

namespace native
{
    list::list(std::vector<std::string> items,
               coord x,
               coord y,
               dim width,
               dim height)
        : wnd(x, y, width, height)
        , _items(std::move(items)) {}

    list::list(const std::vector<std::string> &items,
               const point &position,
               const size &dimensions)
        : list(items,
               position.x,
               position.y,
               dimensions.w,
               dimensions.h) {}

    list::list(const std::vector<std::string> &items,
               const rect &bounds)
        : list(items, bounds.p, bounds.d) {}

    list::~list() {
        destroy();
    }

    const std::vector<std::string> &list::get_items() const {
        return _items;
    }

    list &list::set_items(std::vector<std::string> items) {
        _items = std::move(items);
        if (_selected_index >= static_cast<int>(_items.size()))
            _selected_index = -1;
        if (_created) {
            apply_items();
            apply_selected_index();
        }
        return *this;
    }

    list &list::add_item(const std::string &item) {
        _items.push_back(item);
        if (_created) {
            apply_items();
            apply_selected_index();
        }
        return *this;
    }

    list &list::operator<<(std::string item) {
        return add_item(item);
    }

    list &list::remove_item(std::size_t index) {
        if (index >= _items.size())
            throw std::out_of_range("list item index is out of range");
        _items.erase(_items.begin() +
                     static_cast<std::ptrdiff_t>(index));
        if (_selected_index == static_cast<int>(index))
            _selected_index = -1;
        else if (_selected_index > static_cast<int>(index))
            --_selected_index;
        if (_created) {
            apply_items();
            apply_selected_index();
        }
        return *this;
    }

    list &list::clear_items() {
        _items.clear();
        _selected_index = -1;
        if (_created) {
            apply_items();
            apply_selected_index();
        }
        return *this;
    }

    int list::get_selected_index() const {
        return _selected_index;
    }

    void list::validate_index(int index) const {
        if (index < -1 || index >= static_cast<int>(_items.size()))
            throw std::out_of_range(
                "list selection index is out of range");
    }

    list &list::set_selected_index(int index) {
        validate_index(index);
        if (_selected_index == index)
            return *this;
        _selected_index = index;
        if (_created)
            apply_selected_index();
        return *this;
    }

    void list::on_native_selection(int index) {
        validate_index(index);
        if (_selected_index == index)
            return;
        _selected_index = index;
        on_selection_change.emit(_selected_index);
    }

    void list::draw_control(
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        if (!bounds.d.w || !bounds.d.h)
            return;
        draw_background(graphics, appearance, bounds, state);
        draw_content(graphics, appearance, bounds, state);
        draw_border(graphics, appearance, bounds, state);
        draw_focus(graphics, appearance, bounds, state);
    }

    void list::draw_background(
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &) {
        graphics.set_ink(appearance.get_content_background_color())
            .draw_rect(bounds, true);
    }

    void list::draw_content(
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        const gpx_state restore(graphics);
        graphics.set_clip(bounds);
        const int row_height = std::max(
            1, appearance.get_list_item_height());
        int top = bounds.p.y + 1;
        for (std::size_t index = 0; index < _items.size(); ++index) {
            if (top >= bounds.y2())
                break;
            theme::state row_state = state;
            row_state.selected =
                static_cast<int>(index) == _selected_index;
            appearance.draw_list_item(
                rect(static_cast<coord>(bounds.p.x + 1),
                     static_cast<coord>(top),
                     static_cast<dim>(std::max(
                         0, static_cast<int>(bounds.d.w) - 2)),
                     static_cast<dim>(row_height)),
                _items[index],
                row_state);
            top += row_height;
        }
    }

    void list::draw_border(
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &) {
        graphics.set_pen(1)
            .set_ink(appearance.get_button_border_color())
            .draw_rect(bounds, false);
    }

    void list::draw_focus(
        gpx &,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_focus(bounds, state);
    }
} // namespace native
