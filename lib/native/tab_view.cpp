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

    rect tab_view::get_tab_bounds(std::size_t index) const {
        const tab_item &item = get_item(index);
        const font_t &font = font_t::stock(font_role::control);
        int x = 0;
        for (std::size_t current = 0; current < index; ++current) {
            x += std::max(
                36,
                font.measure_text(_items[current]->_title).width + 20);
        }
        const int natural_width = std::max(
            36, font.measure_text(item._title).width + 20);
        const int available = std::max(
            0, static_cast<int>(_bounds.d.w) - x);
        return rect(
            static_cast<coord>(std::min(
                x,
                static_cast<int>(
                    std::numeric_limits<coord>::max()))),
            0,
            non_negative_dimension(std::min(natural_width, available)),
            non_negative_dimension(_tab_height));
    }

    rect tab_view::get_content_bounds() const {
        const int border = 2;
        const int height = std::max(
            0,
            static_cast<int>(_bounds.d.h) - _tab_height - border);
        return rect(
            static_cast<coord>(border),
            static_cast<coord>(_tab_height),
            non_negative_dimension(
                std::max(0, static_cast<int>(_bounds.d.w)-border*2)),
            non_negative_dimension(height));
    }

    void tab_view::on_native_selection(int index) {
        validate_index(index);
        if (!_items[index]->_enabled || _selected_index == index)
            return;
        _selected_index = index;
        refresh_contents();
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
        gpx &,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_surface(bounds, surface_kind::panel, state);
        appearance.draw_surface(
            get_content_bounds(), surface_kind::content, state);
    }

    void tab_view::draw_tab_background(
        gpx &,
        theme &appearance,
        std::size_t,
        const tab_item &,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_surface(bounds, surface_kind::header, state);
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
                                    : colors.button_text)
            .draw_text(item.get_title(),
                       bounds,
                       {text_align::center,
                        text_valign::center,
                        text_overflow::ellipsis,
                        true});
    }

    void tab_view::draw_tab_border(
        gpx &,
        theme &appearance,
        std::size_t,
        const tab_item &,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_focus(bounds, state);
    }
} // namespace native
