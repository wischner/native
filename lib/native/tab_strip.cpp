//
// Implements the shared library-painted tab strip. Tabs are laid out from
// the first visible index; selecting one scrolls it into view, so a long
// list stays reachable without strip scroll buttons.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>

#include <native/font.h>
#include <native/graphics.h>
#include <native/tab_strip.h>

namespace native
{
    namespace
    {
        constexpr int tab_padding = 10;
        constexpr int tab_inset = 4;
    }

    tab_strip::tab_strip(wnd &owner, window_edge edge, int extent)
        : non_client(owner, edge, extent) {}

    const std::vector<std::string> &tab_strip::get_items() const {
        return _items;
    }

    tab_strip &tab_strip::set_items(std::vector<std::string> items) {
        _items = std::move(items);
        if (_items.empty()) {
            _selected = -1;
            _first = 0;
        } else {
            _selected = std::clamp(_selected, 0,
                                   static_cast<int>(_items.size()) - 1);
            _first = std::min(_first, _items.size() - 1);
        }
        invalidate();
        return *this;
    }

    int tab_strip::get_selected_index() const {
        return _selected;
    }

    tab_strip &tab_strip::set_selected_index(int index) {
        if (index < -1 || index >= static_cast<int>(_items.size()))
            throw std::out_of_range("Tab index is out of range.");
        _selected = index;
        if (index >= 0) {
            const auto wanted = static_cast<std::size_t>(index);
            if (wanted < _first)
                _first = wanted;
            while (_first < wanted && !get_tab_bounds(wanted).d.w)
                ++_first;
        }
        invalidate();
        return *this;
    }

    std::size_t tab_strip::get_first_visible() const {
        return _first;
    }

    tab_strip &tab_strip::set_first_visible(std::size_t index) {
        _first = _items.empty() ? 0 : std::min(index, _items.size() - 1);
        invalidate();
        return *this;
    }

    int tab_strip::tab_length(std::size_t index) const {
        const auto &font = font_t::stock(font_role::control);
        return font.measure_text(_items[index]).width + 2 * tab_padding;
    }

    rect tab_strip::get_tab_bounds(std::size_t index) const {
        const rect strip = get_bounds();
        if (index >= _items.size() || index < _first)
            return rect(0, 0, 0, 0);
        int offset = tab_inset;
        for (std::size_t i = _first; i < index; ++i)
            offset += tab_length(i);
        const int length = tab_length(index);
        if (offset + length > static_cast<int>(strip.d.w))
            return rect(0, 0, 0, 0);
        return rect(static_cast<coord>(offset), 0,
                    static_cast<dim>(length), strip.d.h);
    }

    int tab_strip::tab_at(const point &position) const {
        const rect strip = get_bounds();
        const point local(static_cast<coord>(position.x - strip.p.x),
                          static_cast<coord>(position.y - strip.p.y));
        for (std::size_t i = _first; i < _items.size(); ++i)
            if (get_tab_bounds(i).contains(local))
                return static_cast<int>(i);
        return -1;
    }

    bool tab_strip::pointer_click(const mouse_event &event) {
        if (event.action != mouse_action::press ||
            event.button != mouse_button::left)
            return true;
        const int index = tab_at(event.position);
        if (index < 0)
            return true;
        if (index != _selected) {
            set_selected_index(index);
            on_selection_change.emit(index);
        } else if (event.clicks >= 2) {
            on_activate.emit(index);
        }
        return true;
    }

    void tab_strip::draw(gpx &graphics, const rect &bounds) {
        auto appearance = theme::create(graphics);
        draw_background(graphics, *appearance, bounds);
        for (std::size_t i = _first; i < _items.size(); ++i) {
            const rect local = get_tab_bounds(i);
            if (!local.d.w)
                continue;
            const rect tab(static_cast<coord>(bounds.p.x + local.p.x),
                           bounds.p.y, local.d.w, bounds.d.h);
            theme::state state;
            state.selected = static_cast<int>(i) == _selected;
            draw_tab(graphics, *appearance, tab, _items[i], state);
        }
    }

    void tab_strip::draw_background(gpx &, theme &appearance,
                                    const rect &bounds) {
        appearance.draw_surface(bounds, surface_kind::panel, {});
    }

    void tab_strip::draw_tab(gpx &graphics, theme &appearance,
                             const rect &bounds, const std::string &label,
                             const theme::state &state) {
        const auto saved = graphics.save_state();
        const rect face(bounds.p.x, bounds.p.y, bounds.d.w,
                        static_cast<dim>(bounds.d.h - 2));
        if (state.selected) {
            appearance.draw_surface(face, surface_kind::content, {});
            graphics.set_ink(appearance.get_content_foreground_color())
                .set_pen(1);
            graphics.draw_border(face, border_sides::left | border_sides::right |
                                           border_sides::bottom);
        } else {
            graphics.set_ink(appearance.get_button_shadow_color()).set_pen(1);
            graphics.draw_border(face, border_sides::right);
        }
        graphics.set_font(font_t::stock(font_role::control))
            .set_ink(state.selected ? appearance.get_content_foreground_color()
                                    : appearance.get_button_foreground_color());
        text_layout layout;
        layout.horizontal = text_align::center;
        layout.vertical = text_valign::center;
        graphics.draw_text(label, face, layout);
    }
}
