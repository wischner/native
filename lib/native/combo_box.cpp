//
// Implements backend-neutral combo-box state and events.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>
#include <utility>

#include <native/combo_box.h>
#include <native/graphics.h>

namespace native
{
    combo_box::combo_box(std::vector<std::string> items,
                         combo_box_style style,
                         coord x,
                         coord y,
                         dim width,
                         dim height)
        : wnd(x, y, width, height)
        , _items(std::move(items))
        , _style(style) {}

    combo_box::combo_box(const std::vector<std::string> &items,
                         combo_box_style style,
                         const rect &bounds)
        : combo_box(items, style, bounds.p.x, bounds.p.y,
                    bounds.d.w, bounds.d.h) {}

    combo_box::~combo_box() { destroy(); }

    const std::vector<std::string> &combo_box::get_items() const {
        return _items;
    }

    combo_box &combo_box::set_items(std::vector<std::string> items) {
        _items = std::move(items);
        if (_selected_index >= static_cast<int>(_items.size())) {
            _selected_index = -1;
            if (_style == combo_box_style::drop_down_list)
                _text.clear();
        }
        if (_created) {
            apply_items();
            apply_selected_index();
            apply_text();
        }
        return *this;
    }

    combo_box &combo_box::add_item(const std::string &item) {
        _items.push_back(item);
        if (_created) apply_items();
        return *this;
    }

    combo_box &combo_box::remove_item(std::size_t index) {
        if (index >= _items.size())
            throw std::out_of_range("combo-box item index is out of range");
        _items.erase(_items.begin()+static_cast<std::ptrdiff_t>(index));
        if (_selected_index == static_cast<int>(index)) {
            _selected_index = -1;
            if (_style == combo_box_style::drop_down_list)
                _text.clear();
        } else if (_selected_index > static_cast<int>(index)) {
            --_selected_index;
        }
        if (_created) {
            apply_items();
            apply_selected_index();
            apply_text();
        }
        return *this;
    }

    combo_box &combo_box::clear_items() {
        _items.clear();
        _selected_index = -1;
        if (_style == combo_box_style::drop_down_list)
            _text.clear();
        if (_created) {
            apply_items(); apply_selected_index(); apply_text();
        }
        return *this;
    }

    int combo_box::get_selected_index() const { return _selected_index; }

    void combo_box::validate_index(int index) const {
        if (index < -1 || index >= static_cast<int>(_items.size()))
            throw std::out_of_range(
                "combo-box selection index is out of range");
    }

    combo_box &combo_box::set_selected_index(int index) {
        validate_index(index);
        _selected_index = index;
        if (index >= 0)
            _text = _items[static_cast<std::size_t>(index)];
        else if (_style == combo_box_style::drop_down_list)
            _text.clear();
        if (_created) {
            apply_selected_index(); apply_text();
        }
        return *this;
    }

    const std::string &combo_box::get_text() const { return _text; }

    combo_box &combo_box::set_text(const std::string &text) {
        if (_style == combo_box_style::drop_down_list) {
            const auto found = std::find(_items.begin(), _items.end(), text);
            if (found == _items.end() && !text.empty())
                throw std::invalid_argument(
                    "Selection-only combo text must name an item.");
            _selected_index = found == _items.end() ? -1
                : static_cast<int>(found-_items.begin());
        } else {
            const auto found = std::find(_items.begin(), _items.end(), text);
            _selected_index = found == _items.end() ? -1
                : static_cast<int>(found-_items.begin());
        }
        _text = text;
        if (_created) {
            apply_selected_index(); apply_text();
        }
        return *this;
    }

    combo_box_style combo_box::get_style() const { return _style; }

    combo_box &combo_box::set_style(combo_box_style style) {
        if (_style == style) return *this;
        _style = style;
        if (_style == combo_box_style::drop_down_list &&
            _selected_index < 0)
            _text.clear();
        if (_created) apply_style();
        return *this;
    }

    void combo_box::on_native_selection(int index) {
        validate_index(index);
        _selected_index = index;
        _text = index >= 0 ? _items[static_cast<std::size_t>(index)]
                           : std::string();
        on_selection_change.emit(index);
        on_text_change.emit(_text);
    }

    void combo_box::on_native_text(const std::string &text) {
        if (_style != combo_box_style::editable)
            return;
        _text = text;
        const auto found = std::find(_items.begin(), _items.end(), text);
        _selected_index = found == _items.end() ? -1
            : static_cast<int>(found-_items.begin());
        on_text_change.emit(_text);
    }

    void combo_box::on_native_drop_down(bool open) {
        on_drop_down.emit(open);
    }

    void combo_box::draw_control(gpx &graphics,
                                 theme &appearance,
                                 const rect &bounds,
                                 const theme::state &state) {
        appearance.draw_text_edit_frame(bounds, state);
        const int button_width = std::min<int>(bounds.w(), bounds.h());
        const rect button_bounds(
            static_cast<coord>(bounds.x2()-button_width), bounds.y1(),
            static_cast<dim>(button_width), bounds.h());
        appearance.draw_surface(button_bounds, surface_kind::panel, state);
        const int indicator = std::max(5, button_width/2);
        appearance.draw_disclosure(
            rect(static_cast<coord>(button_bounds.x1()+
                                    (button_width-indicator)/2),
                 static_cast<coord>(button_bounds.y1()+
                                    (bounds.h()-indicator)/2),
                 static_cast<dim>(indicator), static_cast<dim>(indicator)),
            disclosure_state::collapsed, state);
        const theme::palette colors = appearance.native_palette();
        graphics.set_ink(colors.content_text);
        const rect text_bounds(
            static_cast<coord>(bounds.x1()+4), bounds.y1(),
            static_cast<dim>(std::max(0,
                static_cast<int>(bounds.w())-button_width-8)), bounds.h());
        graphics.draw_text(_text, text_bounds,
            {text_align::start, text_valign::center,
             text_overflow::ellipsis, true});
    }
} // namespace native
