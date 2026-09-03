//
// Implements portable tab state, page lifecycle, and fallback painting.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/tab_view.h>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

#include <native/graphics.h>

#include "rotated_text.h"

namespace
{
    native::dim non_negative_dimension(int value) {
        return static_cast<native::dim>(std::clamp(
            value,
            0,
            static_cast<int>(
                std::numeric_limits<native::dim>::max())));
    }

    bool horizontal_tabs(native::tab_placement placement) {
        return placement == native::tab_placement::top ||
               placement == native::tab_placement::bottom;
    }

    std::vector<native::point> tab_outline(
        native::tab_placement placement,
        const native::rect &bounds,
        bool sloped,
        bool selected) {
        const int left = bounds.x1();
        const int right = bounds.x2() - 1;
        const int top = bounds.y1();
        const int bottom = bounds.y2() - 1;
        const auto p = [](int x, int y) {
            return native::point(static_cast<native::coord>(x),
                                 static_cast<native::coord>(y));
        };
        if (sloped) {
            const int offset = selected ? 0 : 1;
            switch (placement) {
            case native::tab_placement::top:
                return {p(left + offset, bottom - offset + 1),
                        p(left + 3, bottom - 2),
                        p(left + 7, top + 3),
                        p(left + 10, top),
                        p(right - 9, top),
                        p(right - 6, top + 3),
                        p(right - 2, bottom - 2),
                        p(right - offset + 1,
                          bottom - offset + 1)};
            case native::tab_placement::bottom:
                return {p(left + offset, top + offset),
                        p(left + 3, top + 3),
                        p(left + 7, bottom - 3),
                        p(left + 10, bottom),
                        p(right - 9, bottom),
                        p(right - 6, bottom - 3),
                        p(right - 2, top + 3),
                        p(right - offset + 1, top + offset)};
            case native::tab_placement::left:
                return {p(right - offset + 1, top + offset),
                        p(right - 2, top + 3),
                        p(left + 3, top + 7),
                        p(left, top + 10),
                        p(left, bottom - 9),
                        p(left + 3, bottom - 6),
                        p(right - 2, bottom - 2),
                        p(right - offset + 1,
                          bottom - offset + 1)};
            case native::tab_placement::right:
                return {p(left + offset, top + offset),
                        p(left + 3, top + 3),
                        p(right - 3, top + 7),
                        p(right, top + 10),
                        p(right, bottom - 9),
                        p(right - 3, bottom - 6),
                        p(left + 3, bottom - 2),
                        p(left + offset, bottom - offset + 1)};
            }
        }
        switch (placement) {
        case native::tab_placement::top:
            return {p(left, bottom),
                    p(left, top + 2),
                    p(left + 2, top),
                    p(right - 2, top),
                    p(right, top + 2),
                    p(right, bottom)};
        case native::tab_placement::bottom:
            return {p(left, top),
                    p(left, bottom - 2),
                    p(left + 2, bottom),
                    p(right - 2, bottom),
                    p(right, bottom - 2),
                    p(right, top)};
        case native::tab_placement::left:
            return {p(right, top),
                    p(left + 2, top),
                    p(left, top + 2),
                    p(left, bottom - 2),
                    p(left + 2, bottom),
                    p(right, bottom)};
        case native::tab_placement::right:
            return {p(left, top),
                    p(right - 2, top),
                    p(right, top + 2),
                    p(right, bottom - 2),
                    p(right - 2, bottom),
                    p(left, bottom)};
        }
        return {};
    }
} // namespace

namespace native
{
    tab_item::tab_item(tab_view &owner,
                       std::string title,
                       wnd &content)
        : _owner(&owner)
        , _title(std::move(title))
        , _content(&content) {}

    const std::string &tab_item::get_title() const {
        return _title;
    }

    tab_item &tab_item::set_title(std::string title) {
        if (_title == title)
            return *this;
        _title = std::move(title);
        if (_owner)
            _owner->refresh();
        return *this;
    }

    bool tab_item::get_enabled() const {
        return _enabled;
    }

    tab_item &tab_item::set_enabled(bool enabled) {
        if (_enabled == enabled)
            return *this;
        _enabled = enabled;
        if (_owner)
            _owner->refresh();
        return *this;
    }

    wnd &tab_item::get_content() const {
        return *_content;
    }

    tab_view::tab_view(coord x,
                       coord y,
                       dim width,
                       dim height)
        : wnd(x, y, width, height) {
        on_wnd_paint.connect([this](wnd_paint_event event) {
            draw(event.g);
            return true;
        });
        on_mouse_click.connect([this](mouse_event event) {
            return handle_click(event);
        });
    }

    tab_view::tab_view(const point &position,
                       const size &dimensions)
        : tab_view(position.x,
                   position.y,
                   dimensions.w,
                   dimensions.h) {}

    tab_view::tab_view(const rect &bounds)
        : tab_view(bounds.p, bounds.d) {}

    tab_view::~tab_view() {
        destroy();
        clear_items();
    }

    std::size_t tab_view::get_item_count() const {
        return _items.size();
    }

    tab_item &tab_view::get_item(std::size_t index) const {
        if (index >= _items.size())
            throw std::out_of_range("tab item index is out of range");
        return *_items[index];
    }

    tab_item &tab_view::add_item(const std::string &title,
                                 wnd &content) {
        if (content.get_created()) {
            throw std::invalid_argument(
                "tab content must be uncreated when added");
        }
        for (const auto &item : _items) {
            if (item->_content == &content) {
                throw std::invalid_argument(
                    "tab content cannot be added twice");
            }
        }

        content.set_parent(this);
        _items.push_back(std::unique_ptr<tab_item>(
            new tab_item(*this, title, content)));
        if (_selected_index < 0)
            _selected_index = 0;
        refresh();
        return *_items.back();
    }

    tab_view &tab_view::remove_item(std::size_t index) {
        tab_item &item = get_item(index);
        detach_item(item);
        _items.erase(_items.begin() +
                     static_cast<std::ptrdiff_t>(index));
        if (_items.empty()) {
            _selected_index = -1;
        } else if (_selected_index == static_cast<int>(index)) {
            _selected_index = std::min(
                static_cast<int>(index),
                static_cast<int>(_items.size()) - 1);
        } else if (_selected_index > static_cast<int>(index)) {
            --_selected_index;
        }
        refresh();
        return *this;
    }

    tab_view &tab_view::clear_items() {
        for (auto &item : _items)
            detach_item(*item);
        _items.clear();
        _selected_index = -1;
        if (_created) {
            apply_items();
            apply_selected_index();
        }
        invalidate();
        return *this;
    }

    int tab_view::get_selected_index() const {
        return _selected_index;
    }

    void tab_view::validate_index(int index) const {
        if (index < 0 || index >= static_cast<int>(_items.size())) {
            throw std::out_of_range(
                "tab selection index is out of range");
        }
    }

    tab_view &tab_view::set_selected_index(int index) {
        validate_index(index);
        if (_selected_index == index)
            return *this;
        _selected_index = index;
        refresh_contents();
        if (_created)
            apply_selected_index();
        invalidate();
        return *this;
    }

    tab_placement tab_view::get_tab_placement() const {
        return _tab_placement;
    }

    tab_view &tab_view::set_tab_placement(tab_placement placement) {
        if (_tab_placement == placement)
            return *this;
        _tab_placement = placement;
        refresh();
        return *this;
    }

    rect tab_view::get_tab_bounds(std::size_t index) const {
        const tab_item &item = get_item(index);
        const font_t &font = font_t::stock(font_role::control);
        int offset = _tab_inset;
        for (std::size_t current = 0; current < index; ++current) {
            const int extent = std::max(
                36,
                font.measure_text(_items[current]->_title).width +
                    _tab_padding);
            offset += std::max(1, extent - _tab_overlap);
        }
        const int natural_extent = std::max(
            36,
            font.measure_text(item._title).width + _tab_padding);
        if (!horizontal_tabs(_tab_placement)) {
            const int available = std::max(
                0, static_cast<int>(_bounds.d.h) - offset);
            const int tab_x = _tab_placement == tab_placement::left
                                  ? 0
                                  : std::max(
                                        0,
                                        static_cast<int>(_bounds.d.w) -
                                            _tab_height);
            return rect(
                static_cast<coord>(tab_x),
                static_cast<coord>(std::min(
                    offset,
                    static_cast<int>(
                        std::numeric_limits<coord>::max()))),
                non_negative_dimension(_tab_height),
                non_negative_dimension(
                    std::min(natural_extent, available)));
        }
        const int available = std::max(
            0, static_cast<int>(_bounds.d.w) - offset);
        const int tab_y = _tab_placement == tab_placement::top
                              ? 0
                              : std::max(
                                    0,
                                    static_cast<int>(_bounds.d.h) -
                                        _tab_height);
        return rect(
            static_cast<coord>(std::min(
                offset,
                static_cast<int>(
                    std::numeric_limits<coord>::max()))),
            static_cast<coord>(tab_y),
            non_negative_dimension(
                std::min(natural_extent, available)),
            non_negative_dimension(_tab_height));
    }

    rect tab_view::get_content_bounds() const {
        if (!horizontal_tabs(_tab_placement)) {
            const int width = std::max(
                0,
                static_cast<int>(_bounds.d.w) - _tab_height -
                    _page_tab_gap - _page_trailing);
            return rect(
                static_cast<coord>(
                    _tab_placement == tab_placement::left
                        ? _tab_height + _page_tab_gap
                        : _page_trailing),
                static_cast<coord>(_page_inset),
                non_negative_dimension(width),
                non_negative_dimension(
                    std::max(0,
                             static_cast<int>(_bounds.d.h) -
                                 _page_inset - _page_trailing)));
        }
        const int height = std::max(
            0,
            static_cast<int>(_bounds.d.h) - _tab_height -
                _page_tab_gap - _page_trailing);
        return rect(
            static_cast<coord>(_page_inset),
            static_cast<coord>(
                _tab_placement == tab_placement::top
                    ? _tab_height + _page_tab_gap
                    : _page_trailing),
            non_negative_dimension(
                std::max(0,
                         static_cast<int>(_bounds.d.w) -
                             _page_inset - _page_trailing)),
            non_negative_dimension(height));
    }

    void tab_view::on_native_selection(int index) {
        validate_index(index);
        if (!_items[index]->_enabled || _selected_index == index)
            return;
        _selected_index = index;
        refresh_contents();
        if (_created)
            apply_selected_index();
        invalidate();
        on_selection_change.emit(index);
    }

    void tab_view::on_bounds_changed() {
        refresh_contents();
        invalidate();
    }

    void tab_view::refresh() {
        if (_created)
            apply_items();
        refresh_contents();
        if (_created)
            apply_selected_index();
        invalidate();
    }

    void tab_view::refresh_contents() {
        rect content_bounds = get_content_bounds();
        if (_content_host_is_page) {
            content_bounds = rect(0, 0,
                                  content_bounds.d.w,
                                  content_bounds.d.h);
        }
        for (std::size_t index = 0; index < _items.size(); ++index) {
            wnd &content = *_items[index]->_content;
            if (static_cast<int>(index) == _selected_index) {
                content.set_bounds(content_bounds);
                if (_created && !content.get_created()) {
                    content.create();
                    content.show();
                }
            } else if (content.get_created()) {
                content.destroy();
            }
        }
    }

    void tab_view::detach_item(tab_item &item) {
        if (!item._content)
            return;
        if (item._content->get_created())
            item._content->destroy();
        if (item._content->get_parent() == this)
            item._content->set_parent(nullptr);
        item._owner = nullptr;
    }

    void tab_view::synchronize_theme_metrics() {
        const font_metrics metrics =
            font_t::stock(font_role::control).get_metrics();
        _tab_height = std::max(20, metrics.height + 8);
        wnd *root = this;
        while (root->get_parent())
            root = root->get_parent();
        try {
            auto appearance = theme::create(root->get_gpx());
            _tab_height = std::max(
                1, appearance->defaults().header_height);
        } catch (const std::runtime_error &) {
            // A native child can provide its precise metric later.
        }
    }

    void tab_view::draw(gpx &graphics) {
        auto saved = graphics.save_state();
        auto appearance = theme::create(graphics);
        const rect bounds(point(0, 0), get_dimensions());
        draw_background(
            graphics, *appearance, bounds, theme::state{});
        if (_sloped_tabs) {
            const auto draw_shape = [&](std::size_t index) {
                const rect tab_bounds = get_tab_bounds(index);
                if (!tab_bounds.d.w || !tab_bounds.d.h)
                    return;
                theme::state state;
                state.selected =
                    static_cast<int>(index) == _selected_index;
                state.pressed = state.selected;
                state.disabled = !_items[index]->_enabled;
                draw_tab_background(graphics,
                                    *appearance,
                                    index,
                                    *_items[index],
                                    tab_bounds,
                                    state);
                draw_tab_border(graphics,
                                *appearance,
                                index,
                                *_items[index],
                                tab_bounds,
                                state);
            };
            for (std::size_t index = _items.size(); index-- > 0;) {
                if (static_cast<int>(index) != _selected_index)
                    draw_shape(index);
            }
            if (_selected_index >= 0)
                draw_shape(static_cast<std::size_t>(_selected_index));
            for (std::size_t index = 0; index < _items.size(); ++index) {
                const rect tab_bounds = get_tab_bounds(index);
                if (!tab_bounds.d.w || !tab_bounds.d.h)
                    continue;
                theme::state state;
                state.selected =
                    static_cast<int>(index) == _selected_index;
                state.pressed = state.selected;
                state.disabled = !_items[index]->_enabled;
                draw_tab_text(graphics,
                              *appearance,
                              index,
                              *_items[index],
                              tab_bounds,
                              state);
            }
            return;
        }
        for (std::size_t index = 0; index < _items.size(); ++index) {
            const rect tab_bounds = get_tab_bounds(index);
            if (!tab_bounds.d.w || !tab_bounds.d.h)
                continue;
            theme::state state;
            state.selected = static_cast<int>(index) == _selected_index;
            state.pressed = state.selected;
            state.disabled = !_items[index]->_enabled;
            draw_tab_background(graphics,
                                *appearance,
                                index,
                                *_items[index],
                                tab_bounds,
                                state);
            draw_tab_text(graphics,
                          *appearance,
                          index,
                          *_items[index],
                          tab_bounds,
                          state);
            draw_tab_border(graphics,
                            *appearance,
                            index,
                            *_items[index],
                            tab_bounds,
                            state);
        }
    }

    bool tab_view::handle_click(const mouse_event &event) {
        if (event.button != mouse_button::left ||
            event.action != mouse_action::release) {
            return false;
        }
        for (std::size_t index = 0; index < _items.size(); ++index) {
            if (_items[index]->_enabled &&
                get_tab_bounds(index).contains(event.position)) {
                on_native_selection(static_cast<int>(index));
                return true;
            }
        }
        return false;
    }

    void tab_view::draw_background(
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        if (_sloped_tabs) {
            const theme::palette colors = appearance.native_palette();
            graphics.set_pen(1)
                .set_ink(colors.button_bg)
                .draw_rect(bounds, true);
            if (!bounds.d.w || !bounds.d.h)
                return;
            const int right = bounds.x2() - 1;
            const int bottom = bounds.y2() - 1;
            const auto p = [](int x, int y) {
                return point(static_cast<coord>(x),
                             static_cast<coord>(y));
            };
            int page_left = 0;
            int page_top = 0;
            int page_right = right;
            int page_bottom = bottom;
            switch (_tab_placement) {
            case tab_placement::top:
                page_top = _tab_height - 1;
                break;
            case tab_placement::bottom:
                page_bottom = static_cast<int>(bounds.d.h) -
                              _tab_height;
                break;
            case tab_placement::left:
                page_left = _tab_height - 1;
                break;
            case tab_placement::right:
                page_right = static_cast<int>(bounds.d.w) -
                             _tab_height;
                break;
            }
            graphics.set_ink(colors.button_highlight)
                .draw_line(p(page_left, page_top),
                           p(page_right, page_top))
                .draw_line(p(page_left, page_top),
                           p(page_left, page_bottom))
                .set_ink(colors.button_border)
                .draw_line(p(page_left, page_bottom),
                           p(page_right, page_bottom))
                .draw_line(p(page_right, page_top),
                           p(page_right, page_bottom));
            if (page_right - page_left > 2 &&
                page_bottom - page_top > 2) {
                graphics.set_ink(colors.button_shadow)
                    .draw_line(p(page_left + 1, page_bottom - 1),
                               p(page_right - 1, page_bottom - 1))
                    .draw_line(p(page_right - 1, page_top + 1),
                               p(page_right - 1, page_bottom - 1));
            }
            graphics.set_ink(colors.button_highlight);
            if (_tab_placement == tab_placement::top)
                graphics.draw_line(p(0, page_top), p(right, page_top));
            else if (_tab_placement == tab_placement::bottom)
                graphics.draw_line(p(0, page_bottom),
                                   p(right, page_bottom));
            else if (_tab_placement == tab_placement::left)
                graphics.draw_line(p(page_left, 0),
                                   p(page_left, bottom));
            else
                graphics.draw_line(p(page_right, 0),
                                   p(page_right, bottom));
            return;
        }
        appearance.draw_surface(bounds, surface_kind::panel, state);
        appearance.draw_surface(
            get_content_bounds(), surface_kind::content, state);
    }

    void tab_view::draw_tab_background(
        gpx &graphics,
        theme &appearance,
        std::size_t,
        const tab_item &,
        const rect &bounds,
        const theme::state &state) {
        const theme::palette colors = appearance.native_palette();
        const int left = bounds.x1();
        const int right = bounds.x2() - 1;
        const int top = bounds.y1();
        const int bottom = bounds.y2() - 1;
        if (right < left || bottom < top)
            return;
        const std::vector<point> outline = tab_outline(
            _tab_placement, bounds, _sloped_tabs, state.selected);
        if (_sloped_tabs) {
            const rgba inactive = _inactive_tab_background.a
                                      ? _inactive_tab_background
                                      : colors.button_bg;
            graphics.set_pen(1)
                .set_ink(state.selected ? colors.button_bg : inactive)
                .draw_polygon(outline, true);
            return;
        }
        const rgba fill = state.pressed
                              ? colors.button_pressed_bg
                              : (state.hot ? colors.button_hot_bg
                                           : colors.button_bg);
        graphics.set_pen(1).set_ink(fill).draw_polygon(outline, true);
    }

    void tab_view::draw_tab_text(
        gpx &graphics,
        theme &appearance,
        std::size_t,
        const tab_item &item,
        const rect &bounds,
        const theme::state &state) {
        const theme::palette colors = appearance.native_palette();
        graphics.set_font(font_t::stock(font_role::control))
            .set_ink(state.disabled ? colors.button_disabled_text
                                    : colors.button_text);
        if (_tab_placement == tab_placement::left ||
            _tab_placement == tab_placement::right) {
            detail::draw_rotated_text(
                graphics,
                item.get_title(),
                bounds,
                _tab_placement == tab_placement::right,
                _sloped_tabs ? 10 : 4);
        } else {
            graphics.draw_text(item.get_title(),
                               bounds,
                               {text_align::center,
                                text_valign::center,
                                text_overflow::ellipsis,
                                true});
        }
    }

    void tab_view::draw_tab_border(
        gpx &graphics,
        theme &appearance,
        std::size_t,
        const tab_item &,
        const rect &bounds,
        const theme::state &state) {
        const theme::palette colors = appearance.native_palette();
        const int left = bounds.x1();
        const int right = bounds.x2() - 1;
        const int top = bounds.y1();
        const int bottom = bounds.y2() - 1;
        if (right < left || bottom < top)
            return;
        const std::vector<point> outline = tab_outline(
            _tab_placement, bounds, _sloped_tabs, state.selected);
        if (_sloped_tabs) {
            const rgba inactive_highlight =
                _inactive_tab_highlight.a
                    ? _inactive_tab_highlight
                    : colors.button_highlight;
            const rgba highlight = state.selected
                                       ? colors.button_highlight
                                       : inactive_highlight;
            graphics.set_pen(1).set_ink(highlight)
                .draw_line(outline[0], outline[1])
                .draw_line(outline[1], outline[2])
                .draw_line(outline[2], outline[3])
                .draw_line(outline[3], outline[4])
                .set_ink(colors.button_shadow)
                .draw_line(outline[4], outline[5])
                .set_ink(colors.button_border)
                .draw_line(outline[5], outline[6])
                .draw_line(outline[6], outline[7])
                .set_ink(state.selected
                             ? colors.button_bg
                             : colors.button_highlight)
                .draw_line(outline[0], outline[7]);
            return;
        }
        graphics.set_pen(1).set_ink(colors.button_border);
        graphics.draw_polyline(outline);
        if (!state.selected && outline.size() >= 2)
            graphics.draw_line(outline.front(), outline.back());
        if (state.focused) {
            rect focus = bounds;
            focus.p.x += 3;
            focus.p.y += 3;
            focus.d.w = non_negative_dimension(
                static_cast<int>(focus.d.w) - 6);
            focus.d.h = non_negative_dimension(
                static_cast<int>(focus.d.h) - 6);
            appearance.draw_focus(focus, state);
        }
    }
} // namespace native
