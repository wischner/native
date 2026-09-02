//
// Implements backend-neutral icon-view items, wrapping geometry,
// selection rebasing, navigation, hit testing, and vertical scrolling.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/icon_view.h>

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

#include "collection_render.h"
#include <native/theme.h>

namespace
{
    native::dim bounded_dimension(int value) {
        return static_cast<native::dim>(std::clamp(
            value,
            0,
            static_cast<int>(
                std::numeric_limits<native::dim>::max())));
    }
} // namespace

namespace native
{
    icon_view::icon_view(std::vector<icon_view_item> items,
                         coord x,
                         coord y,
                         dim width,
                         dim height)
        : wnd(x, y, width, height)
        , _items(std::move(items)) {
        on_wnd_paint.connect([this](wnd_paint_event event) {
            detail::draw_icon_view(*this, event.g);
            return true;
        });
        on_mouse_click.connect([this](mouse_event event) {
            return event.button == mouse_button::left &&
                   event.action == mouse_action::release &&
                   detail::handle_icon_view_click(
                       *this, event.position);
        });
        on_mouse_wheel.connect([this](mouse_wheel_event event) {
            if (event.direction != wheel_direction::vertical)
                return false;
            on_native_scroll(-event.delta);
            return true;
        });
    }

    icon_view::icon_view(const std::vector<icon_view_item> &items,
                         const point &position,
                         const size &dimensions)
        : icon_view(items,
                    position.x,
                    position.y,
                    dimensions.w,
                    dimensions.h) {}

    icon_view::icon_view(const std::vector<icon_view_item> &items,
                         const rect &bounds)
        : icon_view(items, bounds.p, bounds.d) {}

    icon_view::~icon_view() {
        destroy();
    }

    const std::vector<icon_view_item> &icon_view::get_items() const {
        return _items;
    }

    icon_view &icon_view::set_items(
        std::vector<icon_view_item> items) {
        const std::uint64_t selected_id =
            _selected_index >= 0
                ? _items[static_cast<std::size_t>(_selected_index)].id
                : 0;
        _items = std::move(items);
        if (selected_id != 0) {
            const auto selected = std::find_if(
                _items.begin(),
                _items.end(),
                [selected_id](const icon_view_item &item) {
                    return item.id == selected_id;
                });
            _selected_index = selected == _items.end()
                                  ? -1
                                  : static_cast<int>(
                                        selected - _items.begin());
        } else if (_selected_index >= static_cast<int>(_items.size())) {
            _selected_index = -1;
        }
        _scroll_offset = std::min(_scroll_offset,
                                  maximum_scroll_offset());
        if (_created) {
            apply_items();
            apply_selected_index();
            apply_scroll_offset();
        }
        invalidate();
        return *this;
    }

    icon_view &icon_view::add_item(icon_view_item item) {
        _items.push_back(std::move(item));
        if (_created)
            apply_items();
        invalidate();
        return *this;
    }

    icon_view &icon_view::remove_item(std::size_t index) {
        if (index >= _items.size())
            throw std::out_of_range(
                "icon_view item index is out of range");
        _items.erase(_items.begin() +
                     static_cast<std::ptrdiff_t>(index));
        if (_selected_index == static_cast<int>(index))
            _selected_index = -1;
        else if (_selected_index > static_cast<int>(index))
            --_selected_index;
        _scroll_offset = std::min(_scroll_offset,
                                  maximum_scroll_offset());
        if (_created) {
            apply_items();
            apply_selected_index();
            apply_scroll_offset();
        }
        invalidate();
        return *this;
    }

    icon_view &icon_view::clear_items() {
        _items.clear();
        _selected_index = -1;
        _scroll_offset = 0;
        if (_created) {
            apply_items();
            apply_selected_index();
            apply_scroll_offset();
        }
        invalidate();
        return *this;
    }

    icon_view &icon_view::set_icon_size(size dimensions) {
        if (!dimensions.w || !dimensions.h)
            throw std::invalid_argument(
                "icon_view icon dimensions must be non-zero");
        _icon_size = dimensions;
        _scroll_offset = std::min(_scroll_offset,
                                  maximum_scroll_offset());
        if (_created) {
            apply_icon_size();
            apply_scroll_offset();
        }
        invalidate();
        return *this;
    }

    size icon_view::get_icon_size() const {
        return _icon_size;
    }

    icon_view &icon_view::set_label_mode(icon_view_label_mode mode) {
        if (_label_mode == mode)
            return *this;
        _label_mode = mode;
        _scroll_offset = std::min(_scroll_offset,
                                  maximum_scroll_offset());
        if (_created) {
            apply_label_mode();
            apply_scroll_offset();
        }
        invalidate();
        return *this;
    }

    icon_view_label_mode icon_view::get_label_mode() const {
        return _label_mode;
    }

    int icon_view::get_selected_index() const {
        return _selected_index;
    }

    void icon_view::validate_index(int index) const {
        if (index < -1 || index >= static_cast<int>(_items.size())) {
            throw std::out_of_range(
                "icon_view selection index is out of range");
        }
    }

    icon_view &icon_view::set_selected_index(int index) {
        validate_index(index);
        if (_selected_index == index)
            return *this;
        _selected_index = index;
        ensure_selection_visible();
        if (_created) {
            apply_selected_index();
            apply_scroll_offset();
        }
        invalidate();
        return *this;
    }

    int icon_view::get_scroll_offset() const {
        return _scroll_offset;
    }

    icon_view &icon_view::set_scroll_offset(int offset) {
        const int clamped = std::clamp(
            offset, 0, maximum_scroll_offset());
        if (_scroll_offset == clamped)
            return *this;
        _scroll_offset = clamped;
        if (_created)
            apply_scroll_offset();
        invalidate();
        return *this;
    }

    int icon_view::item_width() const {
        const int image_width = _icon_size.w + _item_padding * 2;
        if (_label_mode == icon_view_label_mode::beside) {
            return std::max(_minimum_item_width * 2,
                            image_width + _minimum_item_width);
        }
        return std::max(_minimum_item_width, image_width);
    }

    int icon_view::item_height() const {
        const int image_height = _icon_size.h + _item_padding * 2;
        if (_label_mode == icon_view_label_mode::below) {
            return image_height + _label_gap + _label_height * 2;
        }
        if (_label_mode == icon_view_label_mode::beside)
            return std::max(image_height, _label_height * 2 +
                                              _item_padding * 2);
        return image_height;
    }

    int icon_view::column_count() const {
        const int usable = std::max(
            1, static_cast<int>(_bounds.d.w) - _item_gap * 2);
        return std::max(1, (usable + _item_gap) /
                               (item_width() + _item_gap));
    }

    rect icon_view::get_item_bounds(std::size_t index) const {
        if (index >= _items.size())
            throw std::out_of_range(
                "icon_view item index is out of range");
        const int columns = column_count();
        const int column = static_cast<int>(index) % columns;
        const int row = static_cast<int>(index) / columns;
        const int x = _item_gap + column * (item_width() + _item_gap);
        const int y = _item_gap + row * (item_height() + _item_gap) -
                      _scroll_offset;
        return rect(static_cast<coord>(x),
                    static_cast<coord>(std::clamp(
                        y,
                        static_cast<int>(
                            std::numeric_limits<coord>::min()),
                        static_cast<int>(
                            std::numeric_limits<coord>::max()))),
                    bounded_dimension(item_width()),
                    bounded_dimension(item_height()));
    }

    int icon_view::item_at(point position) const {
        if (position.x < 0 || position.y < 0 ||
            position.x >= static_cast<int>(_bounds.d.w) ||
            position.y >= static_cast<int>(_bounds.d.h)) {
            return -1;
        }
        for (std::size_t index = 0; index < _items.size(); ++index) {
            if (get_item_bounds(index).contains(position))
                return static_cast<int>(index);
        }
        return -1;
    }

    size icon_view::get_content_dimensions() const {
        const int columns = column_count();
        const int rows = _items.empty()
                             ? 0
                             : (static_cast<int>(_items.size()) +
                                columns - 1) /
                                   columns;
        const int width = _item_gap * 2 +
                          columns * item_width() +
                          std::max(0, columns - 1) * _item_gap;
        const int height = rows == 0
                               ? 0
                               : _item_gap * 2 +
                                     rows * item_height() +
                                     (rows - 1) * _item_gap;
        return size(bounded_dimension(width),
                    bounded_dimension(height));
    }

    int icon_view::maximum_scroll_offset() const {
        return std::max(0,
                        static_cast<int>(
                            get_content_dimensions().h) -
                            static_cast<int>(_bounds.d.h));
    }

    void icon_view::on_native_selection(int index) {
        validate_index(index);
        if (index >= 0 && !_items[index].enabled)
            return;
        if (_selected_index == index)
            return;
        _selected_index = index;
        ensure_selection_visible();
        if (_created) {
            apply_selected_index();
            apply_scroll_offset();
        }
        invalidate();
        on_selection_change.emit(index);
    }

    void icon_view::on_native_activate(int index) {
        validate_index(index);
        if (index >= 0 && _items[index].enabled)
            on_item_activate.emit(index);
    }

    int icon_view::navigated_index(
        icon_view_navigation navigation) const {
        if (_items.empty())
            return -1;
        int current = _selected_index >= 0 ? _selected_index : 0;
        int direction = 1;
        const int columns = column_count();
        const int page_rows = std::max(
            1,
            static_cast<int>(_bounds.d.h) /
                std::max(1, item_height() + _item_gap));
        switch (navigation) {
        case icon_view_navigation::left:
            --current;
            direction = -1;
            break;
        case icon_view_navigation::right:
            ++current;
            break;
        case icon_view_navigation::up:
            current -= columns;
            direction = -1;
            break;
        case icon_view_navigation::down:
            current += columns;
            break;
        case icon_view_navigation::home:
            current = 0;
            direction = 1;
            break;
        case icon_view_navigation::end:
            current = static_cast<int>(_items.size()) - 1;
            direction = -1;
            break;
        case icon_view_navigation::page_up:
            current -= columns * page_rows;
            direction = -1;
            break;
        case icon_view_navigation::page_down:
            current += columns * page_rows;
            break;
        }
        current = std::clamp(
            current, 0, static_cast<int>(_items.size()) - 1);
        while (current >= 0 &&
               current < static_cast<int>(_items.size()) &&
               !_items[current].enabled) {
            current += direction;
        }
        if (current >= 0 && current < static_cast<int>(_items.size()))
            return current;
        return _selected_index >= 0 &&
                       _items[static_cast<std::size_t>(_selected_index)]
                           .enabled
                   ? _selected_index
                   : -1;
    }

    void icon_view::on_native_navigation(
        icon_view_navigation navigation) {
        on_native_selection(navigated_index(navigation));
    }

    void icon_view::on_native_scroll(int delta) {
        set_scroll_offset(_scroll_offset + delta);
    }

    bool icon_view::get_focused() const {
        return _focused;
    }

    void icon_view::on_native_focus(bool focused) {
        if (_focused == focused)
            return;
        _focused = focused;
        invalidate();
    }

    void icon_view::ensure_selection_visible() {
        if (_selected_index < 0)
            return;
        const rect selected = get_item_bounds(
            static_cast<std::size_t>(_selected_index));
        if (selected.p.y < 0)
            _scroll_offset += selected.p.y;
        else if (selected.y2() > static_cast<int>(_bounds.d.h))
            _scroll_offset += selected.y2() - _bounds.d.h;
        _scroll_offset = std::clamp(
            _scroll_offset, 0, maximum_scroll_offset());
    }

    void icon_view::on_bounds_changed() {
        _scroll_offset = std::min(_scroll_offset,
                                  maximum_scroll_offset());
        if (_created) {
            apply_items();
            apply_scroll_offset();
        }
        invalidate();
    }

    void icon_view::synchronize_theme_metrics() {
        const font_metrics font =
            font_t::stock(font_role::icon_label).get_metrics();
        _label_height = std::max(1, font.height);
        wnd *root = this;
        while (root->get_parent())
            root = root->get_parent();
        try {
            gpx &graphics = root->get_gpx();
            auto painter = theme::create(graphics);
            const theme::metrics values = painter->defaults();
            _item_padding = std::max(
                0, std::max(values.icon_view_padding_x,
                            values.icon_view_padding_y));
            _item_gap = std::max(
                0, std::max(values.icon_view_item_gap_x,
                            values.icon_view_item_gap_y));
            _label_gap = std::max(0, values.icon_view_label_gap);
            _minimum_item_width = std::max(
                1, values.icon_view_min_item_width);
            auto saved = graphics.save_state();
            graphics.set_font(font_t::stock(font_role::icon_label));
            _label_height = std::max(
                1, graphics.get_font_metrics().height);
        } catch (const std::runtime_error &) {
            // Some native hosts acquire their drawable only when the
            // parent is first mapped. The stock font and portable
            // spacing defaults remain valid during early creation.
        }
        _scroll_offset = std::min(_scroll_offset,
                                  maximum_scroll_offset());
        ensure_selection_visible();
    }

    void icon_view::draw_background(
        gpx &,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_surface(bounds, surface_kind::content, state);
    }

    void icon_view::draw_item_background(
        gpx &,
        theme &appearance,
        std::size_t,
        const icon_view_item &,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_selection(bounds, selection_shape::tile, state);
    }

    void icon_view::draw_item_image(
        gpx &graphics,
        theme &,
        std::size_t,
        const icon_view_item &item,
        const rect &bounds,
        const theme::state &) {
        if (item.image)
            graphics.draw_img(
                *item.image, bounds, image_filter::linear);
    }

    void icon_view::draw_item_label(
        gpx &graphics,
        theme &appearance,
        std::size_t,
        const icon_view_item &item,
        const rect &bounds,
        const theme::state &state) {
        if (_label_mode == icon_view_label_mode::hidden)
            return;
        const theme::palette colors = appearance.native_palette();
        graphics.set_font(font_t::stock(font_role::icon_label))
            .set_ink(
                state.disabled
                    ? colors.selection_inactive_text
                    : (state.selected ? colors.selection_text
                                      : colors.content_text))
            .draw_text(
                item.text,
                bounds,
                text_layout{
                    _label_mode == icon_view_label_mode::below
                        ? text_align::center
                        : text_align::start,
                    text_valign::top,
                    text_overflow::ellipsis,
                    true});
    }

    void icon_view::draw_item_focus(
        gpx &,
        theme &appearance,
        std::size_t,
        const icon_view_item &,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_focus(bounds, state);
    }

    void icon_view::draw_scrollbar(
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
