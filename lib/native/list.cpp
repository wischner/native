//
// Implements backend-neutral list items and selection state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

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
        gpx &,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_list(
            bounds, _items, _selected_index, state);
    }
} // namespace native
