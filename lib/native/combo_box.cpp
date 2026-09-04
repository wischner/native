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
#include <native/font.h>
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
                         const point &position,
                         const size &dimensions)
        : combo_box(items, style, position.x, position.y,
                    dimensions.w, dimensions.h) {}

    combo_box::combo_box(const std::vector<std::string> &items,
                         combo_box_style style,
                         const rect &bounds)
        : combo_box(items, style, bounds.p, bounds.d) {}

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
        } else if (_selected_index >= 0) {
            _text = _items[static_cast<std::size_t>(_selected_index)];
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

    combo_box &combo_box::operator<<(std::string item) {
        return add_item(item);
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
        const int previous_index = _selected_index;
        const std::string previous_text = _text;
        _selected_index = index;
        _text = index >= 0 ? _items[static_cast<std::size_t>(index)]
                           : std::string();
        if (_selected_index != previous_index)
            on_selection_change.emit(index);
        if (_text != previous_text)
            on_text_change.emit(_text);
    }

    void combo_box::on_native_text(const std::string &text) {
        if (_style != combo_box_style::editable)
            return;
        const std::string previous_text = _text;
        _text = text;
        const auto found = std::find(_items.begin(), _items.end(), text);
        _selected_index = found == _items.end() ? -1
            : static_cast<int>(found-_items.begin());
        if (_text != previous_text)
            on_text_change.emit(_text);
    }

    void combo_box::on_native_drop_down(bool open) {
        on_drop_down.emit(open);
    }

    void combo_box::draw_control(gpx &graphics,
                                 theme &appearance,
                                 const rect &bounds,
                                 const theme::state &state) {
        if (!bounds.d.w || !bounds.d.h)
            return;
        draw_background(graphics, appearance, bounds, state);
        draw_border(graphics, appearance, bounds, state);
        draw_text(graphics, appearance, bounds, state);
        draw_indicator(graphics, appearance, bounds, state);
        draw_focus(graphics, appearance, bounds, state);
    }

    void combo_box::draw_background(
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &) {
        graphics.set_ink(appearance.get_content_background_color())
            .draw_rect(bounds, true);
    }

    void combo_box::draw_border(
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &) {
        graphics.set_pen(1)
            .set_ink(appearance.get_button_border_color())
            .draw_rect(bounds, false);
    }

    void combo_box::draw_text(
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        const int button_width = std::min<int>(bounds.w(), bounds.h());
        const rect text_bounds(
            static_cast<coord>(bounds.x1() + 4), bounds.y1(),
            static_cast<dim>(std::max(
                0, static_cast<int>(bounds.w()) - button_width - 8)),
            bounds.h());
        graphics.set_font(font_t::stock(font_role::control))
            .set_ink(state.disabled
                ? appearance.get_button_disabled_foreground_color()
                : appearance.get_content_foreground_color());
        graphics.draw_text(
            _text,
            text_bounds,
            {text_align::start,
             text_valign::center,
             text_overflow::ellipsis,
             true});
    }

    void combo_box::draw_indicator(
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        const int button_width = std::min<int>(bounds.w(), bounds.h());
        const rect button_bounds(
            static_cast<coord>(bounds.x2()-button_width+1),
            static_cast<coord>(bounds.y1()+1),
            static_cast<dim>(std::max(0, button_width-2)),
            static_cast<dim>(std::max(0, static_cast<int>(bounds.h())-2)));
        if (!button_bounds.w() || !button_bounds.h())
            return;
        // A combo uses a quiet white button and a compact filled arrow.
        // Reusing the tree disclosure glyph made the control look like an
        // expander and produced an unusually heavy arrow on SDL.
        appearance.draw_surface(
            button_bounds, surface_kind::content, state);
        const int half = std::max(
            4, std::min<int>(button_bounds.w(), button_bounds.h()) / 6);
        const int center_x = button_bounds.x1() + button_bounds.w() / 2;
        const int center_y = button_bounds.y1() + button_bounds.h() / 2;
        const theme::palette colors = appearance.native_palette();
        graphics.set_ink(state.disabled ? colors.button_disabled_text
                                        : colors.button_text)
            .draw_polygon(
                {{static_cast<coord>(center_x - half),
                  static_cast<coord>(center_y - 2)},
                 {static_cast<coord>(center_x + half),
                  static_cast<coord>(center_y - 2)},
                 {static_cast<coord>(center_x),
                  static_cast<coord>(center_y + 3)}},
                true);
    }

    void combo_box::draw_focus(
        gpx &,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_focus(bounds, state);
    }
} // namespace native
