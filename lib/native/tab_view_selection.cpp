//
// Implements the opt-in label-only tab model without a second tab renderer.
// Ordinary page-owning tab views retain their existing lifecycle.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native/tab_view.h>
#include <native/font.h>
#include <algorithm>
#include <stdexcept>

namespace native
{
    tab_view &tab_view::set_selection_only(bool enabled) {
        if (_selection_only == enabled) return *this;
        if (get_created() || !_items.empty())
            throw std::logic_error(
                "Set selection-only mode before creating or populating tabs.");
        _selection_only = enabled;
        _first_visible = 0;
        return *this;
    }

    bool tab_view::get_selection_only() const { return _selection_only; }

    tab_item &tab_view::add_item(const std::string &title) {
        if (!_selection_only)
            throw std::logic_error("Label-only items require selection-only mode.");
        _items.push_back(std::unique_ptr<tab_item>(
            new tab_item(*this, title, nullptr)));
        if (_selected_index < 0) _selected_index = 0;
        refresh();
        return *_items.back();
    }

    int tab_view::get_tab_extent() const { return _tab_height + 1; }

    void tab_view::on_native_activation(int index) {
        validate_index(index);
        if (_items[index]->_enabled)
            on_activate.emit(index);
    }

    void tab_view::reveal_selection() {
        if (!_selection_only || _selected_index < 0) return;
        const auto selected = static_cast<std::size_t>(_selected_index);
        _first_visible = std::min(_first_visible, selected);
        const bool horizontal = _tab_placement == tab_placement::top ||
                                _tab_placement == tab_placement::bottom;
        const auto &font = font_t::stock(font_role::control);
        const int natural = std::max(36,
            font.measure_text(_items[selected]->_title).width + _tab_padding);
        while (_first_visible < selected) {
            const auto bounds = get_tab_bounds(selected);
            if ((horizontal ? bounds.w() : bounds.h()) >= natural) break;
            ++_first_visible;
        }
    }
}
