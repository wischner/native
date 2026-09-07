//
// Implements edge toolbars using menu theme primitives and a canvas
// input surface. Sticky groups are scoped to each bar.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native/toolbar.h>

#include <algorithm>
#include <set>
#include <stdexcept>

#include <native/canvas.h>
#include <native/font.h>
#include <native/graphics.h>

namespace native
{
    toolbar::toolbar(wnd &owner, window_edge edge, int extent)
        : non_client(owner, edge, extent), _surface(std::make_unique<canvas>()) {
        _surface->_non_client_surface = true;
        _surface->set_parent(&owner);
        _surface->set_horizontal_scrollbar_policy(scrollbar_policy::never);
        _surface->set_vertical_scrollbar_policy(scrollbar_policy::never);
        _surface->on_wnd_paint.connect([this](wnd_paint_event event) {
            paint(event); return true;
        });
        _surface->on_mouse_click.connect([this](mouse_event event) {
            mouse_click(event); return true;
        });
        _surface->on_mouse_move.connect([this](point position) {
            mouse_move(position); return true;
        });
        synchronize();
    }

    toolbar::~toolbar() = default;

    const std::vector<tool_item> &toolbar::get_items() const { return _items; }

    toolbar &toolbar::set_icon_size(size dimensions) {
        if (!dimensions.w || !dimensions.h || dimensions.w > 32760 ||
            dimensions.h > 32760)
            throw std::invalid_argument("Toolbar icon dimensions must be positive and fit screen coordinates.");
        const bool vertical = get_edge() == window_edge::left ||
                              get_edge() == window_edge::right;
        const int padding = std::max(4, get_extent() -
            int(vertical ? _icon_size.w : _icon_size.h));
        _icon_size = dimensions;
        set_extent((vertical ? dimensions.w : dimensions.h) + padding);
        repaint();
        return *this;
    }

    size toolbar::get_icon_size() const { return _icon_size; }

    toolbar &toolbar::set_items(std::vector<tool_item> items) {
        std::set<std::string> ids, selected_groups;
        for (const auto &item : items) {
            if (item.kind == tool_kind::separator) continue;
            if (item.id.empty() || !ids.insert(item.id).second)
                throw std::invalid_argument("Toolbar tools need unique nonempty IDs.");
            if (item.kind == tool_kind::button && item.checked)
                throw std::invalid_argument("A momentary tool cannot be checked.");
            if (item.kind == tool_kind::exclusive && item.checked &&
                !selected_groups.insert(item.group).second)
                throw std::invalid_argument("Only one tool per group can be selected.");
        }
        _items = std::move(items);
        _pressed.clear();
        _hot = -1;
        on_configuration_changed();
        repaint();
        return *this;
    }

    toolbar &toolbar::add_item(tool_item item) {
        auto items = _items;
        items.push_back(std::move(item));
        return set_items(std::move(items));
    }

    toolbar &toolbar::operator<<(tool_item item) {
        return add_item(std::move(item));
    }

    std::size_t toolbar::find_item(const std::string &id) const {
        for (std::size_t index = 0; index < _items.size(); ++index)
            if (_items[index].kind != tool_kind::separator && _items[index].id == id)
                return index;
        throw std::out_of_range("Unknown toolbar tool: " + id);
    }

    toolbar &toolbar::set_enabled(const std::string &id, bool enabled) {
        _items[find_item(id)].enabled = enabled;
        if (!enabled && _pressed == id) _pressed.clear();
        repaint();
        return *this;
    }

    toolbar &toolbar::set_checked(const std::string &id, bool checked) {
        auto &item = _items[find_item(id)];
        if (item.kind == tool_kind::button)
            throw std::invalid_argument("Only sticky toolbar tools can be checked.");
        if (checked && item.kind == tool_kind::exclusive)
            for (auto &peer : _items)
                if (peer.kind == tool_kind::exclusive && peer.group == item.group)
                    peer.checked = false;
        item.checked = checked;
        repaint();
        return *this;
    }

    bool toolbar::get_checked(const std::string &id) const {
        return _items[find_item(id)].checked;
    }

    bool toolbar::on_native_command(const std::string &id) {
        const auto item = _items[find_item(id)];
        if (!item.enabled) return false;
        if (item.kind == tool_kind::exclusive && item.checked) return false;
        if (item.kind != tool_kind::button)
            set_checked(id, item.kind == tool_kind::exclusive || !item.checked);
        const tool_command command{id, get_checked(id)};
        on_command.emit(command);
        return true;
    }

    void toolbar::synchronize() {
        if (!_surface) return;
        auto *owner = get_owner();
        const auto bounds = get_bounds();
        if (!owner || !owner->get_created() || !get_visible() ||
            !bounds.w() || !bounds.h()) {
            _surface->destroy();
            _pressed.clear();
            _hot = -1;
            return;
        }
        const auto old = _surface->get_bounds();
        if (old.x1() != bounds.x1() || old.y1() != bounds.y1() ||
            old.w() != bounds.w() || old.h() != bounds.h())
            _surface->set_bounds(bounds);
        if (!_surface->get_created()) {
            _surface->create();
            _surface->show();
        }
    }

    void toolbar::on_configuration_changed() {
        const bool vertical = get_edge() == window_edge::left ||
                              get_edge() == window_edge::right;
        if (get_extent() && std::any_of(_items.begin(), _items.end(),
                [](const tool_item &item) { return bool(item.icon); })) {
            const int minimum = (vertical ? _icon_size.w : _icon_size.h) + 4;
            if (get_extent() < minimum) {
                set_extent(minimum);
                return;
            }
        }
        synchronize();
        repaint();
    }
    void toolbar::draw(gpx &, const rect &) {}

    void toolbar::repaint() {
        if (_surface) _surface->invalidate();
        invalidate();
    }

    std::vector<rect> toolbar::item_bounds() const {
        const auto bounds = get_bounds();
        const bool vertical = get_edge() == window_edge::left ||
                              get_edge() == window_edge::right;
        std::vector<rect> result;
        int offset = 2;
        for (const auto &item : _items) {
            int length = 8;
            if (item.kind != tool_kind::separator) {
                length = vertical ? 24 : 12 + static_cast<int>(item.text.size()) * 8;
                if (_surface->get_created()) {
                    const auto &font = font_t::stock(font_role::control);
                    length = vertical ? font.get_metrics().height + 6
                                      : font.measure_text(item.text).width + 12;
                }
                if (item.icon) {
                    if (vertical) length = std::max(length, int(_icon_size.h) + 4);
                    else length = item.text.empty() ? _icon_size.w + 4 : length + _icon_size.w + 4;
                }
                length = std::max(length, 20);
            }
            const int available = (vertical ? bounds.h() : bounds.w()) - 2 - offset;
            const int visible = std::max(0, std::min(length, available));
            result.emplace_back(vertical ? 2 : std::min(offset, 32767),
                vertical ? std::min(offset, 32767) : 2,
                vertical ? std::max(0, int(bounds.w()) - 4) : visible,
                vertical ? visible : std::max(0, int(bounds.h()) - 4));
            offset = std::min(32767, offset + length);
        }
        return result;
    }

    int toolbar::item_at(point position) const {
        const auto bounds = item_bounds();
        for (std::size_t index = 0; index < bounds.size(); ++index)
            if (_items[index].kind != tool_kind::separator &&
                _items[index].enabled && bounds[index].contains(position))
                return static_cast<int>(index);
        return -1;
    }

    void toolbar::mouse_move(point position) {
        const int hot = item_at(position);
        if (hot == _hot) return;
        _hot = hot;
        repaint();
    }

    void toolbar::mouse_click(mouse_event event) {
        if (event.button != mouse_button::left) return;
        mouse_move(event.position);
        if (event.action == mouse_action::press) {
            _pressed = _hot >= 0 ? _items[_hot].id : std::string{};
            repaint();
        } else {
            const auto id = std::move(_pressed);
            _pressed.clear();
            repaint();
            if (_hot >= 0 && !id.empty() && _items[_hot].id == id)
                on_native_command(id);
        }
    }

    void toolbar::paint(wnd_paint_event event) {
        auto saved = event.g.save_state();
        auto appearance = theme::create(event.g);
        const auto dimensions = _surface->get_dimensions();
        appearance->draw_menu_bar(rect(0, 0, dimensions.w, dimensions.h));
        event.g.set_pen(1).set_ink(appearance->native_palette().menu_bar_bg)
            .draw_border(rect(0, 0, dimensions.w, dimensions.h),
                static_cast<border_sides>(unsigned(border_sides::all) ^ unsigned(get_border_sides())));
        const auto bounds = item_bounds();
        for (std::size_t index = 0; index < _items.size(); ++index) {
            if (!bounds[index].w() || !bounds[index].h()) continue;
            theme::state state;
            state.disabled = !_items[index].enabled;
            state.pressed = _hot == static_cast<int>(index) &&
                            !_pressed.empty() && _pressed == _items[index].id;
            // Classic menu feedback highlights a held or selected tool.
            // An idle pointer must not leave a momentary tool looking sticky.
            state.hot = state.pressed;
            state.selected = _items[index].checked;
            draw_tool(event.g, *appearance, bounds[index], _items[index], state);
        }
    }

    void toolbar::draw_tool(gpx &graphics, theme &appearance, const rect &bounds,
                            const tool_item &item, const theme::state &state) {
        if (item.kind == tool_kind::separator) {
            const bool vertical = get_edge() == window_edge::left ||
                                  get_edge() == window_edge::right;
            appearance.draw_separator(bounds, vertical ? separator_orientation::horizontal
                                                        : separator_orientation::vertical);
            return;
        }
        appearance.draw_toolbar_button(bounds, "", state);
        const auto &font = font_t::stock(font_role::control);
        graphics.set_font(font);
        const int text_width = item.text.empty() ? 0 : font.measure_text(item.text).width;
        const int icon_width = item.icon ? int(_icon_size.w) : 0;
        const int gap = item.icon && !item.text.empty() ? 4 : 0;
        const int content_width = icon_width + gap + text_width;
        const int left = bounds.x1() + std::max(2, (int(bounds.w()) - content_width) / 2);
        if (item.icon) {
            const int width = std::min(int(_icon_size.w), int(bounds.w()) - 4);
            const int height = std::min(int(_icon_size.h), int(bounds.h()));
            const rect destination(left, bounds.y1() + (bounds.h() - height) / 2,
                                   std::max(0, width), height);
            const auto *pixels = item.icon->pixels();
            const auto count = std::size_t(item.icon->w()) * item.icon->h();
            const bool mask = std::all_of(pixels, pixels + count,
                [](rgba pixel) { return !pixel.a || (!pixel.r && !pixel.g && !pixel.b); });
            if (mask && (state.disabled || state.hot || state.pressed || state.selected)) {
                img tinted(item.icon->w(), item.icon->h());
                const rgba ink = state.disabled ? appearance.native_palette().menu_disabled_text
                                               : appearance.native_palette().menu_hot_text;
                for (std::size_t index = 0; index < count; ++index)
                    tinted.pixels()[index] = rgba(ink.r, ink.g, ink.b, pixels[index].a);
                graphics.draw_img(tinted, destination);
            } else graphics.draw_img(*item.icon, destination);
        }
        if (!item.text.empty()) {
            graphics.set_ink(state.disabled ? appearance.native_palette().menu_disabled_text
                    : (state.hot || state.pressed || state.selected)
                        ? appearance.native_palette().menu_hot_text
                        : appearance.native_palette().menu_text)
                .draw_text(item.text, rect(left + icon_width + gap, bounds.y1(),
                    std::max(0, bounds.x2() - left - icon_width - gap - 2), bounds.h()),
                    {text_align::start, text_valign::center, text_overflow::ellipsis, true});
        }
    }
}
