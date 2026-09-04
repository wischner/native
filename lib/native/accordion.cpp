//
// Implements backend-neutral accordion state, borrowed-content
// lifecycle, single/multiple expansion, and section geometry.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/accordion.h>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

#include <native/graphics.h>
#include <native/theme.h>

#include "collection_render.h"

namespace
{
    native::dim non_negative_dimension(int value) {
        return static_cast<native::dim>(std::clamp(
            value,
            0,
            static_cast<int>(
                std::numeric_limits<native::dim>::max())));
    }
} // namespace

namespace native
{
    accordion_section::accordion_section(std::string title_value,
                                         wnd &content_value)
        : title(std::move(title_value))
        , icon(nullptr)
        , content(&content_value) {}

    accordion_section::accordion_section(std::string title_value,
                                         const img &icon_value,
                                         wnd &content_value)
        : title(std::move(title_value))
        , icon(&icon_value)
        , content(&content_value) {}

    accordion_item::accordion_item(accordion &owner,
                                   std::string title,
                                   const img *icon,
                                   wnd &content)
        : _owner(&owner)
        , _title(std::move(title))
        , _icon(icon)
        , _content(&content)
        , _preferred_height(content.get_dimensions().h) {}

    const std::string &accordion_item::get_title() const {
        return _title;
    }

    accordion_item &accordion_item::set_title(std::string title) {
        _title = std::move(title);
        if (_owner)
            _owner->refresh();
        return *this;
    }

    bool accordion_item::get_expanded() const {
        return _expanded;
    }

    accordion_item &accordion_item::set_expanded(bool expanded) {
        if (_owner) {
            for (std::size_t index = 0;
                 index < _owner->get_item_count();
                 ++index) {
                if (&_owner->get_item(index) == this) {
                    _owner->set_item_expanded(index, expanded);
                    break;
                }
            }
        }
        return *this;
    }

    bool accordion_item::get_enabled() const {
        return _enabled;
    }

    accordion_item &accordion_item::set_enabled(bool enabled) {
        if (_enabled == enabled)
            return *this;
        _enabled = enabled;
        if (_owner)
            _owner->refresh();
        return *this;
    }

    wnd &accordion_item::get_content() const {
        return *_content;
    }

    const img *accordion_item::get_icon() const {
        return _icon;
    }

    accordion::accordion(coord x,
                         coord y,
                         dim width,
                         dim height)
        : collection_view(x, y, width, height) {
        on_wnd_paint.connect([this](wnd_paint_event event) {
            detail::draw_accordion(*this, event.g);
            return true;
        });
        on_mouse_click.connect([this](mouse_event event) {
            return event.button == mouse_button::left &&
                   event.action == mouse_action::release &&
                   detail::handle_accordion_click(
                       *this, event.position);
        });
    }

    accordion::accordion(const point &position,
                         const size &dimensions)
        : accordion(position.x,
                    position.y,
                    dimensions.w,
                    dimensions.h) {}

    accordion::accordion(const rect &bounds)
        : accordion(bounds.p, bounds.d) {}

    accordion::~accordion() {
        destroy();
        clear_items();
    }

    accordion &accordion::set_mode(accordion_mode mode) {
        if (_mode == mode)
            return *this;
        _mode = mode;
        if (_mode == accordion_mode::single) {
            bool found = false;
            for (auto &item : _items) {
                if (item->_expanded && !found)
                    found = true;
                else
                    item->_expanded = false;
            }
        }
        refresh();
        return *this;
    }

    accordion_mode accordion::get_mode() const {
        return _mode;
    }

    accordion &accordion::set_border_visible(bool visible) {
        if (_border_visible == visible)
            return *this;
        _border_visible = visible;
        refresh();
        return *this;
    }

    bool accordion::get_border_visible() const {
        return _border_visible;
    }

    std::size_t accordion::get_item_count() const {
        return _items.size();
    }

    accordion_item &accordion::get_item(std::size_t index) const {
        if (index >= _items.size())
            throw std::out_of_range(
                "accordion item index is out of range");
        return *_items[index];
    }

    accordion_item &accordion::add_item(const std::string &title,
                                        wnd &content) {
        if (content.get_created()) {
            throw std::invalid_argument(
                "accordion content must be uncreated when added");
        }
        for (const auto &item : _items) {
            if (item->_content == &content) {
                throw std::invalid_argument(
                    "accordion content cannot be added twice");
            }
        }
        content.set_parent(this);
        auto item = std::unique_ptr<accordion_item>(
            new accordion_item(*this, title, nullptr, content));
        if (_items.empty())
            item->_expanded = true;
        _items.push_back(std::move(item));
        refresh();
        return *_items.back();
    }

    accordion_item &accordion::add_item(const std::string &title,
                                        const img &icon,
                                        wnd &content) {
        accordion_item &item = add_item(title, content);
        item._icon = &icon;
        refresh();
        return item;
    }

    accordion &accordion::operator<<(accordion_section section) {
        if (section.icon)
            add_item(section.title, *section.icon, *section.content);
        else
            add_item(section.title, *section.content);
        return *this;
    }

    accordion &accordion::remove_item(std::size_t index) {
        accordion_item &item = get_item(index);
        const bool removed_expanded = item._expanded;
        detach_item(item);
        _items.erase(_items.begin() +
                     static_cast<std::ptrdiff_t>(index));
        if (_focused_index == static_cast<int>(index)) {
            _focused_index = _items.empty()
                                 ? -1
                                 : std::min(
                                       static_cast<int>(index),
                                       static_cast<int>(_items.size()) - 1);
        } else if (_focused_index > static_cast<int>(index)) {
            --_focused_index;
        }
        if (removed_expanded && _mode == accordion_mode::single &&
            !_items.empty()) {
            const std::size_t replacement =
                std::min(index, _items.size() - 1);
            _items[replacement]->_expanded = true;
        }
        refresh();
        return *this;
    }

    accordion &accordion::clear_items() {
        for (auto &item : _items)
            detach_item(*item);
        _items.clear();
        _focused_index = -1;
        if (_created)
            apply_items();
        invalidate();
        return *this;
    }

    int accordion::get_expanded_index() const {
        for (std::size_t index = 0; index < _items.size(); ++index) {
            if (_items[index]->_expanded)
                return static_cast<int>(index);
        }
        return -1;
    }

    void accordion::validate_index(int index, bool allow_none) const {
        const int minimum = allow_none ? -1 : 0;
        if (index < minimum ||
            index >= static_cast<int>(_items.size())) {
            throw std::out_of_range(
                "accordion expanded index is out of range");
        }
    }

    accordion &accordion::set_expanded_index(int index) {
        validate_index(index, true);
        bool changed = false;
        for (std::size_t current = 0;
             current < _items.size();
             ++current) {
            const bool expanded =
                static_cast<int>(current) == index;
            if (_items[current]->_expanded != expanded) {
                _items[current]->_expanded = expanded;
                changed = true;
            }
        }
        if (changed)
            refresh();
        else if (_created)
            apply_items();
        return *this;
    }

    void accordion::set_item_expanded(std::size_t index,
                                      bool expanded) {
        get_item(index);
        if (_items[index]->_expanded == expanded)
            return;
        if (_mode == accordion_mode::single && expanded) {
            for (auto &item : _items)
                item->_expanded = false;
        }
        _items[index]->_expanded = expanded;
        refresh();
    }

    rect accordion::get_header_bounds(std::size_t index) const {
        get_item(index);
        int y = 0;
        for (std::size_t current = 0; current < index; ++current) {
            y += _header_height;
            if (_items[current]->_expanded) {
                if (_mode == accordion_mode::single) {
                    y += std::max(
                        0,
                        static_cast<int>(_bounds.d.h) -
                            static_cast<int>(_items.size()) *
                                _header_height);
                } else {
                    y += _items[current]->_preferred_height;
                }
            }
        }
        return rect(0,
                    static_cast<coord>(std::clamp(
                        y,
                        static_cast<int>(
                            std::numeric_limits<coord>::min()),
                        static_cast<int>(
                            std::numeric_limits<coord>::max()))),
                    _bounds.d.w,
                    non_negative_dimension(_header_height));
    }

    rect accordion::get_content_bounds(std::size_t index) const {
        accordion_item &item = get_item(index);
        const rect header = get_header_bounds(index);
        const int inset = _border_visible ? 1 : 0;
        const dim width = non_negative_dimension(
            static_cast<int>(_bounds.d.w) - inset * 2);
        if (!item._expanded) {
            return rect(static_cast<coord>(inset),
                        static_cast<coord>(header.y2()),
                        width,
                        0);
        }

        int height = item._preferred_height;
        if (_mode == accordion_mode::single) {
            height = std::max(
                0,
                static_cast<int>(_bounds.d.h) -
                    static_cast<int>(_items.size()) * _header_height);
        }
        height = std::min(
            height,
            std::max(0,
                     static_cast<int>(_bounds.d.h) - inset -
                         header.y2()));
        return rect(static_cast<coord>(inset),
                    static_cast<coord>(header.y2()),
                    width,
                    non_negative_dimension(height));
    }

    void accordion::on_native_toggle(std::size_t index) {
        accordion_item &item = get_item(index);
        if (!item._enabled)
            return;
        _focused_index = static_cast<int>(index);
        const bool expanded = !item._expanded;
        set_item_expanded(index, expanded);
        on_expanded_change.emit(expanded ? static_cast<int>(index)
                                         : -1);
    }

    int accordion::get_focused_index() const {
        return _focused_index;
    }

    void accordion::on_native_focus(bool focused) {
        const bool focus_changed = get_focused() != focused;
        custom_control::on_native_focus(focused);

        int next = -1;
        if (focused && !_items.empty()) {
            const int candidate = std::clamp(
                _focused_index,
                0,
                static_cast<int>(_items.size()) - 1);
            if (_items[candidate]->_enabled) {
                next = candidate;
            } else {
                for (std::size_t index = 0;
                     index < _items.size();
                     ++index) {
                    if (_items[index]->_enabled) {
                        next = static_cast<int>(index);
                        break;
                    }
                }
            }
        }
        if (_focused_index == next)
            return;
        _focused_index = next;
        if (!focus_changed)
            invalidate();
    }

    void accordion::on_native_navigation(
        accordion_navigation navigation) {
        if (_items.empty())
            return;
        int next = std::clamp(_focused_index,
                              0,
                              static_cast<int>(_items.size()) - 1);
        int direction = 0;
        switch (navigation) {
        case accordion_navigation::previous:
            --next;
            direction = -1;
            break;
        case accordion_navigation::next:
            ++next;
            direction = 1;
            break;
        case accordion_navigation::first:
            next = 0;
            direction = 1;
            break;
        case accordion_navigation::last:
            next = static_cast<int>(_items.size()) - 1;
            direction = -1;
            break;
        case accordion_navigation::toggle:
            if (_items[next]->_enabled)
                on_native_toggle(static_cast<std::size_t>(next));
            return;
        }
        while (next >= 0 && next < static_cast<int>(_items.size()) &&
               !_items[next]->_enabled) {
            next += direction;
        }
        if (next < 0 || next >= static_cast<int>(_items.size()))
            return;
        if (_focused_index != next) {
            _focused_index = next;
            invalidate();
        }
    }

    void accordion::refresh() {
        for (std::size_t index = 0; index < _items.size(); ++index) {
            accordion_item &item = *_items[index];
            if (item._expanded) {
                item._content->set_bounds(get_content_bounds(index));
                if (_created && !item._content->get_created()) {
                    item._content->create();
                    item._content->show();
                }
            } else if (item._content->get_created()) {
                item._content->destroy();
            }
        }
        if (_created)
            apply_items();
        invalidate();
    }

    void accordion::detach_item(accordion_item &item) {
        if (!item._content)
            return;
        if (item._content->get_created())
            item._content->destroy();
        if (item._content->get_parent() == this)
            item._content->set_parent(nullptr);
        item._owner = nullptr;
    }

    void accordion::on_bounds_changed() {
        refresh();
    }

    void accordion::synchronize_theme_metrics() {
        custom_control::synchronize_theme_metrics();
        _header_height = std::max(1, _theme_metrics.header_height);
    }

    void accordion::draw_background(
        gpx &,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_surface(bounds, surface_kind::panel, state);
    }

    void accordion::draw_header_background(
        gpx &,
        theme &appearance,
        std::size_t,
        const accordion_item &,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_surface(bounds, surface_kind::header, state);
    }

    void accordion::draw_header_disclosure(
        gpx &,
        theme &appearance,
        std::size_t,
        const accordion_item &item,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_disclosure(
            bounds,
            item.get_expanded() ? disclosure_state::expanded
                                : disclosure_state::collapsed,
            state);
    }

    void accordion::draw_header_image(
        gpx &graphics,
        theme &,
        std::size_t,
        const accordion_item &item,
        const rect &bounds,
        const theme::state &) {
        if (const img *image = item.get_icon())
            graphics.draw_img(*image, bounds, image_filter::linear);
    }

    void accordion::draw_header_text(
        gpx &graphics,
        theme &appearance,
        std::size_t,
        const accordion_item &item,
        const rect &bounds,
        const theme::state &state) {
        const theme::palette colors = appearance.native_palette();
        graphics.set_font(font_t::stock(font_role::control))
            .set_ink(state.disabled ? colors.button_disabled_text
                                    : colors.button_text)
            .draw_text(
                item.get_title(),
                bounds,
                text_layout{text_align::start,
                            text_valign::center,
                            text_overflow::ellipsis,
                            true});
    }

    void accordion::draw_header_border(
        gpx &,
        theme &appearance,
        std::size_t,
        const accordion_item &,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_focus(bounds, state);
        const int extent = std::max(
            1, appearance.defaults().separator_extent);
        appearance.draw_separator(
            rect(bounds.p.x,
                 static_cast<coord>(bounds.y2() - extent),
                 bounds.d.w,
                 static_cast<dim>(extent)),
            separator_orientation::horizontal);
    }

    void accordion::draw_border(
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &) {
        if (!_border_visible)
            return;
        graphics.set_pen(1)
            .set_ink(appearance.get_button_border_color())
            .draw_rect(bounds, false);
    }
} // namespace native
