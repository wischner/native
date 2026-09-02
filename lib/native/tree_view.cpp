//
// Implements backend-neutral tree ownership, stable-ID selection,
// expansion, flattened geometry, classic navigation, and scrolling.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/tree_view.h>

#include <algorithm>
#include <functional>
#include <limits>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include <native/theme.h>

#include "collection_render.h"

namespace
{
    native::dim bounded_dimension(int value) {
        return static_cast<native::dim>(std::clamp(
            value,
            0,
            static_cast<int>(
                std::numeric_limits<native::dim>::max())));
    }

    native::coord bounded_coordinate(int value) {
        return static_cast<native::coord>(std::clamp(
            value,
            static_cast<int>(
                std::numeric_limits<native::coord>::min()),
            static_cast<int>(
                std::numeric_limits<native::coord>::max())));
    }
} // namespace

namespace native
{
    tree_view_item::tree_view_item() = default;

    tree_view_item::tree_view_item(
        std::string item_text,
        std::shared_ptr<const img> item_image,
        tree_item_id item_id,
        std::vector<tree_view_item> item_children,
        bool item_expanded,
        bool item_enabled)
        : text(std::move(item_text))
        , image(std::move(item_image))
        , id(item_id)
        , children(std::move(item_children))
        , expanded(item_expanded)
        , enabled(item_enabled) {}

    tree_view::tree_view(std::vector<tree_view_item> items,
                         coord x,
                         coord y,
                         dim width,
                         dim height)
        : wnd(x, y, width, height)
        , _items(std::move(items)) {
        validate_items(_items);
        on_wnd_paint.connect([this](wnd_paint_event event) {
            detail::draw_tree_view(*this, event.g);
            return true;
        });
        on_mouse_click.connect([this](mouse_event event) {
            return event.button == mouse_button::left &&
                   event.action == mouse_action::release &&
                   detail::handle_tree_view_click(
                       *this, event.position);
        });
        on_mouse_wheel.connect([this](mouse_wheel_event event) {
            if (event.direction != wheel_direction::vertical)
                return false;
            on_native_scroll(-event.delta);
            return true;
        });
    }

    tree_view::tree_view(const std::vector<tree_view_item> &items,
                         const point &position,
                         const size &dimensions)
        : tree_view(items,
                    position.x,
                    position.y,
                    dimensions.w,
                    dimensions.h) {}

    tree_view::tree_view(const std::vector<tree_view_item> &items,
                         const rect &bounds)
        : tree_view(items, bounds.p, bounds.d) {}

    tree_view::~tree_view() {
        destroy();
    }

    const std::vector<tree_view_item> &tree_view::get_items() const {
        return _items;
    }

    void tree_view::validate_items(
        const std::vector<tree_view_item> &items) const {
        std::unordered_set<tree_item_id> ids;
        std::function<void(const std::vector<tree_view_item> &)> visit;
        visit = [&ids, &visit](
                    const std::vector<tree_view_item> &branches) {
            for (const tree_view_item &item : branches) {
                if (item.id == invalid_tree_item_id) {
                    throw std::invalid_argument(
                        "tree_view item IDs must be non-zero");
                }
                if (!ids.insert(item.id).second) {
                    throw std::invalid_argument(
                        "tree_view item IDs must be unique");
                }
                visit(item.children);
            }
        };
        visit(items);
    }

    tree_view &tree_view::set_items(
        std::vector<tree_view_item> items) {
        validate_items(items);
        _items = std::move(items);
        if (_selected_item != invalid_tree_item_id &&
            !contains_item(_selected_item)) {
            _selected_item = invalid_tree_item_id;
        }
        _scroll_offset = std::min(_scroll_offset,
                                  maximum_scroll_offset());
        if (_created) {
            apply_items();
            apply_selection();
            apply_scroll_offset();
        }
        invalidate();
        return *this;
    }

    tree_view &tree_view::add_item(tree_view_item item,
                                   tree_item_id parent_id) {
        std::vector<tree_view_item> candidate = _items;
        if (parent_id == invalid_tree_item_id) {
            candidate.push_back(std::move(item));
        } else {
            std::function<tree_view_item *(
                std::vector<tree_view_item> &)> find;
            find = [parent_id, &find](
                       std::vector<tree_view_item> &branches)
                -> tree_view_item * {
                for (tree_view_item &branch : branches) {
                    if (branch.id == parent_id)
                        return &branch;
                    if (tree_view_item *found = find(branch.children))
                        return found;
                }
                return nullptr;
            };
            tree_view_item *parent = find(candidate);
            if (!parent)
                throw std::out_of_range(
                    "tree_view parent ID was not found");
            parent->children.push_back(std::move(item));
        }
        return set_items(std::move(candidate));
    }

    tree_view &tree_view::remove_item(tree_item_id id) {
        validate_id(id, false);
        std::function<bool(std::vector<tree_view_item> &)> erase;
        erase = [id, &erase](std::vector<tree_view_item> &branches) {
            for (auto iterator = branches.begin();
                 iterator != branches.end();
                 ++iterator) {
                if (iterator->id == id) {
                    branches.erase(iterator);
                    return true;
                }
                if (erase(iterator->children))
                    return true;
            }
            return false;
        };
        erase(_items);
        if (_selected_item != invalid_tree_item_id &&
            !contains_item(_selected_item)) {
            _selected_item = invalid_tree_item_id;
        }
        _scroll_offset = std::min(_scroll_offset,
                                  maximum_scroll_offset());
        if (_created) {
            apply_items();
            apply_selection();
            apply_scroll_offset();
        }
        invalidate();
        return *this;
    }

    tree_view &tree_view::clear_items() {
        _items.clear();
        _selected_item = invalid_tree_item_id;
        _scroll_offset = 0;
        if (_created) {
            apply_items();
            apply_selection();
            apply_scroll_offset();
        }
        invalidate();
        return *this;
    }

    tree_view_item *tree_view::find_item(tree_item_id id) {
        std::function<tree_view_item *(
            std::vector<tree_view_item> &)> find;
        find = [id, &find](std::vector<tree_view_item> &branches)
            -> tree_view_item * {
            for (tree_view_item &item : branches) {
                if (item.id == id)
                    return &item;
                if (tree_view_item *found = find(item.children))
                    return found;
            }
            return nullptr;
        };
        return id == invalid_tree_item_id ? nullptr : find(_items);
    }

    const tree_view_item *tree_view::find_item(
        tree_item_id id) const {
        return const_cast<tree_view *>(this)->find_item(id);
    }

    tree_view_item *tree_view::find_parent(tree_item_id id) {
        std::function<tree_view_item *(
            std::vector<tree_view_item> &)> find;
        find = [id, &find](std::vector<tree_view_item> &branches)
            -> tree_view_item * {
            for (tree_view_item &item : branches) {
                const auto child = std::find_if(
                    item.children.begin(),
                    item.children.end(),
                    [id](const tree_view_item &candidate) {
                        return candidate.id == id;
                    });
                if (child != item.children.end())
                    return &item;
                if (tree_view_item *found = find(item.children))
                    return found;
            }
            return nullptr;
        };
        return id == invalid_tree_item_id ? nullptr : find(_items);
    }

    const tree_view_item *tree_view::find_parent(
        tree_item_id id) const {
        return const_cast<tree_view *>(this)->find_parent(id);
    }

    const tree_view_item &tree_view::get_item(tree_item_id id) const {
        const tree_view_item *item = find_item(id);
        if (!item)
            throw std::out_of_range(
                "tree_view item ID was not found");
        return *item;
    }

    bool tree_view::contains_item(tree_item_id id) const {
        return find_item(id) != nullptr;
    }

    void tree_view::validate_id(tree_item_id id,
                                bool allow_none) const {
        if (id == invalid_tree_item_id) {
            if (allow_none)
                return;
            throw std::out_of_range(
                "tree_view item ID must be non-zero");
        }
        if (!contains_item(id))
            throw std::out_of_range(
                "tree_view item ID was not found");
    }

    tree_item_id tree_view::get_selected_item() const {
        return _selected_item;
    }

    tree_view &tree_view::set_selected_item(tree_item_id id) {
        validate_id(id, true);
        if (id != invalid_tree_item_id && !get_item(id).enabled) {
            throw std::invalid_argument(
                "tree_view cannot select a disabled item");
        }
        if (_selected_item == id)
            return *this;
        _selected_item = id;
        ensure_item_visible(id);
        if (_created) {
            apply_selection();
            apply_scroll_offset();
        }
        invalidate();
        return *this;
    }

    tree_view &tree_view::set_expanded(tree_item_id id,
                                       bool expanded) {
        tree_view_item *item = find_item(id);
        if (!item)
            throw std::out_of_range(
                "tree_view item ID was not found");
        if (item->children.empty())
            expanded = false;
        if (item->expanded == expanded)
            return *this;
        item->expanded = expanded;
        if (!expanded && _selected_item != id) {
            const auto visible = visible_items();
            const bool selection_visible = std::any_of(
                visible.begin(),
                visible.end(),
                [this](const tree_view_visible_item &entry) {
                    return entry.id == _selected_item;
                });
            if (!selection_visible)
                _selected_item = id;
        }
        _scroll_offset = std::min(_scroll_offset,
                                  maximum_scroll_offset());
        ensure_item_visible(_selected_item);
        if (_created) {
            apply_expansion(id, expanded);
            apply_selection();
            apply_scroll_offset();
        }
        invalidate();
        return *this;
    }

    bool tree_view::get_expanded(tree_item_id id) const {
        return get_item(id).expanded;
    }

    tree_view &tree_view::reveal_item(tree_item_id id) {
        validate_id(id, false);
        std::vector<tree_item_id> ancestors;
        tree_item_id current = id;
        while (const tree_view_item *parent = find_parent(current)) {
            ancestors.push_back(parent->id);
            current = parent->id;
        }
        bool changed = false;
        for (tree_item_id ancestor : ancestors) {
            tree_view_item *item = find_item(ancestor);
            if (item && !item->expanded) {
                item->expanded = true;
                changed = true;
                if (_created)
                    apply_expansion(ancestor, true);
            }
        }
        ensure_item_visible(id);
        if (_created)
            apply_scroll_offset();
        if (changed || id != invalid_tree_item_id)
            invalidate();
        return *this;
    }

    tree_view &tree_view::set_icon_size(size dimensions) {
        if (!dimensions.w || !dimensions.h)
            throw std::invalid_argument(
                "tree_view icon dimensions must be non-zero");
        if (_icon_size.w == dimensions.w &&
            _icon_size.h == dimensions.h)
            return *this;
        _icon_size = dimensions;
        _row_height = std::max(
            _row_height, static_cast<int>(_icon_size.h) + 4);
        _scroll_offset = std::min(_scroll_offset,
                                  maximum_scroll_offset());
        if (_created) {
            apply_items();
            apply_selection();
            apply_scroll_offset();
        }
        invalidate();
        return *this;
    }

    size tree_view::get_icon_size() const {
        return _icon_size;
    }

    tree_view &tree_view::set_lines_visible(bool visible) {
        _lines_visible_explicit = true;
        if (_lines_visible == visible)
            return *this;
        _lines_visible = visible;
        if (_created)
            apply_items();
        invalidate();
        return *this;
    }

    bool tree_view::get_lines_visible() const {
        return _lines_visible;
    }

    tree_view &tree_view::set_presentation(
        tree_view_presentation presentation) {
        if (_presentation == presentation)
            return *this;
        _presentation = presentation;
        if (_created) {
            apply_items();
            apply_selection();
            apply_scroll_offset();
        }
        invalidate();
        return *this;
    }

    tree_view_presentation tree_view::get_presentation() const {
        return _presentation;
    }

    std::vector<tree_view_visible_item>
    tree_view::visible_items() const {
        std::vector<tree_view_visible_item> result;
        std::function<void(const std::vector<tree_view_item> &,
                           std::size_t)> append;
        append = [&result, &append](
                     const std::vector<tree_view_item> &items,
                     std::size_t depth) {
            for (const tree_view_item &item : items) {
                result.push_back({item.id, depth});
                if (item.expanded)
                    append(item.children, depth + 1);
            }
        };
        append(_items, 0);
        return result;
    }

    std::size_t tree_view::get_visible_item_count() const {
        return visible_items().size();
    }

    tree_view_visible_item tree_view::get_visible_item(
        std::size_t index) const {
        const auto visible = visible_items();
        if (index >= visible.size())
            throw std::out_of_range(
                "tree_view visible index is out of range");
        return visible[index];
    }

    rect tree_view::get_row_bounds(std::size_t index) const {
        if (index >= get_visible_item_count())
            throw std::out_of_range(
                "tree_view visible index is out of range");
        const int y = static_cast<int>(index) * _row_height -
                      _scroll_offset;
        return rect(0,
                    bounded_coordinate(y),
                    _bounds.d.w,
                    bounded_dimension(_row_height));
    }

    rect tree_view::get_disclosure_bounds(
        std::size_t index) const {
        const tree_view_visible_item visible = get_visible_item(index);
        const rect row = get_row_bounds(index);
        const int x = _horizontal_padding +
                      static_cast<int>(visible.depth) * _indent_width;
        const int y = row.p.y +
                      (static_cast<int>(row.d.h) -
                       _disclosure_size) /
                          2;
        return rect(bounded_coordinate(x),
                    bounded_coordinate(y),
                    bounded_dimension(_disclosure_size),
                    bounded_dimension(_disclosure_size));
    }

    tree_view_hit tree_view::hit_test(point position) const {
        if (position.x < 0 || position.y < 0 ||
            position.x >= static_cast<int>(_bounds.d.w) ||
            position.y >= static_cast<int>(_bounds.d.h)) {
            return {};
        }
        const int content_y = position.y + _scroll_offset;
        if (content_y < 0 || _row_height <= 0)
            return {};
        const std::size_t index = static_cast<std::size_t>(
            content_y / _row_height);
        if (index >= get_visible_item_count())
            return {};
        const tree_view_visible_item visible = get_visible_item(index);
        const tree_view_item &item = get_item(visible.id);
        if (!item.children.empty() &&
            get_disclosure_bounds(index).contains(position)) {
            return {visible.id, tree_view_hit_part::disclosure};
        }
        return {visible.id, tree_view_hit_part::row};
    }

    tree_item_id tree_view::item_at(point position) const {
        return hit_test(position).id;
    }

    int tree_view::maximum_scroll_offset() const {
        const std::size_t count = get_visible_item_count();
        const std::size_t height = static_cast<std::size_t>(
            std::max(1, _row_height));
        const std::size_t maximum = static_cast<std::size_t>(
            std::numeric_limits<int>::max());
        const int content = count > maximum / height
                                ? std::numeric_limits<int>::max()
                                : static_cast<int>(count * height);
        return std::max(0,
                        content - static_cast<int>(_bounds.d.h));
    }

    int tree_view::get_scroll_offset() const {
        return _scroll_offset;
    }

    tree_view &tree_view::set_scroll_offset(int offset) {
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

    void tree_view::ensure_item_visible(tree_item_id id) {
        if (id == invalid_tree_item_id)
            return;
        const auto visible = visible_items();
        const auto found = std::find_if(
            visible.begin(),
            visible.end(),
            [id](const tree_view_visible_item &item) {
                return item.id == id;
            });
        if (found == visible.end())
            return;
        const std::size_t index = static_cast<std::size_t>(
            found - visible.begin());
        const int top = static_cast<int>(index) * _row_height;
        const int bottom = top + _row_height;
        if (top < _scroll_offset)
            _scroll_offset = top;
        else if (bottom > _scroll_offset +
                              static_cast<int>(_bounds.d.h)) {
            _scroll_offset = bottom - _bounds.d.h;
        }
        _scroll_offset = std::clamp(
            _scroll_offset, 0, maximum_scroll_offset());
    }

    void tree_view::on_native_selection(tree_item_id id) {
        validate_id(id, true);
        if (id != invalid_tree_item_id && !get_item(id).enabled)
            return;
        if (_selected_item == id)
            return;
        _selected_item = id;
        ensure_item_visible(id);
        if (_created) {
            apply_selection();
            apply_scroll_offset();
        }
        invalidate();
        on_selection_change.emit(id);
    }

    void tree_view::on_native_expansion(tree_item_id id,
                                        bool expanded) {
        tree_view_item *item = find_item(id);
        if (!item)
            throw std::out_of_range(
                "tree_view item ID was not found");
        if (item->children.empty())
            return;
        if (!item->enabled)
            return;
        if (item->expanded == expanded)
            return;
        item->expanded = expanded;
        if (!expanded && _selected_item != id) {
            const auto visible = visible_items();
            const bool selection_visible = std::any_of(
                visible.begin(),
                visible.end(),
                [this](const tree_view_visible_item &entry) {
                    return entry.id == _selected_item;
                });
            if (!selection_visible)
                _selected_item = id;
        }
        _scroll_offset = std::min(_scroll_offset,
                                  maximum_scroll_offset());
        ensure_item_visible(_selected_item);
        if (_created) {
            apply_expansion(id, expanded);
            apply_selection();
            apply_scroll_offset();
        }
        invalidate();
        on_expanded_change.emit(id, expanded);
    }

    void tree_view::on_native_navigation(
        tree_view_navigation navigation) {
        const auto visible = visible_items();
        if (visible.empty())
            return;
        auto selectable = [this](tree_item_id id) {
            return get_item(id).enabled;
        };
        auto selected = std::find_if(
            visible.begin(),
            visible.end(),
            [this](const tree_view_visible_item &item) {
                return item.id == _selected_item;
            });
        std::size_t index = selected == visible.end()
                                ? 0
                                : static_cast<std::size_t>(
                                      selected - visible.begin());

        if (navigation == tree_view_navigation::left) {
            const tree_view_item &item = get_item(visible[index].id);
            if (!item.children.empty() && item.expanded) {
                on_native_expansion(item.id, false);
            } else if (const tree_view_item *parent =
                           find_parent(item.id)) {
                on_native_selection(parent->id);
            }
            return;
        }
        if (navigation == tree_view_navigation::right) {
            const tree_view_item &item = get_item(visible[index].id);
            if (!item.children.empty() && !item.expanded) {
                on_native_expansion(item.id, true);
            } else if (!item.children.empty()) {
                const auto child = std::find_if(
                    item.children.begin(),
                    item.children.end(),
                    [](const tree_view_item &candidate) {
                        return candidate.enabled;
                    });
                if (child != item.children.end())
                    on_native_selection(child->id);
            }
            return;
        }
        if (navigation == tree_view_navigation::toggle) {
            const tree_view_item &item = get_item(visible[index].id);
            if (!item.children.empty())
                on_native_expansion(item.id, !item.expanded);
            return;
        }
        if (navigation == tree_view_navigation::activate) {
            on_native_activate(visible[index].id);
            return;
        }

        int next = static_cast<int>(index);
        int direction = 1;
        const int page = std::max(
            1,
            static_cast<int>(_bounds.d.h) /
                std::max(1, _row_height));
        switch (navigation) {
        case tree_view_navigation::up:
            --next;
            direction = -1;
            break;
        case tree_view_navigation::down:
            ++next;
            break;
        case tree_view_navigation::home:
            next = 0;
            break;
        case tree_view_navigation::end:
            next = static_cast<int>(visible.size()) - 1;
            direction = -1;
            break;
        case tree_view_navigation::page_up:
            next -= page;
            direction = -1;
            break;
        case tree_view_navigation::page_down:
            next += page;
            break;
        default:
            return;
        }
        next = std::clamp(
            next, 0, static_cast<int>(visible.size()) - 1);
        while (next >= 0 &&
               next < static_cast<int>(visible.size()) &&
               !selectable(visible[static_cast<std::size_t>(next)].id)) {
            next += direction;
        }
        if (next >= 0 && next < static_cast<int>(visible.size())) {
            on_native_selection(
                visible[static_cast<std::size_t>(next)].id);
        }
    }

    void tree_view::on_native_double_click(tree_item_id id) {
        validate_id(id, false);
        const tree_view_item &item = get_item(id);
        if (!item.enabled)
            return;
        if (!item.children.empty())
            on_native_expansion(id, !item.expanded);
        on_item_activate.emit(id);
    }

    void tree_view::on_native_activate(tree_item_id id) {
        validate_id(id, false);
        if (get_item(id).enabled)
            on_item_activate.emit(id);
    }

    void tree_view::on_native_scroll(int delta) {
        set_scroll_offset(_scroll_offset + delta);
    }

    bool tree_view::get_focused() const {
        return _focused;
    }

    void tree_view::on_native_focus(bool focused) {
        if (_focused == focused)
            return;
        _focused = focused;
        invalidate();
    }

    void tree_view::on_bounds_changed() {
        _scroll_offset = std::min(_scroll_offset,
                                  maximum_scroll_offset());
        ensure_item_visible(_selected_item);
        if (_created) {
            apply_items();
            apply_selection();
            apply_scroll_offset();
        }
        invalidate();
    }

    void tree_view::synchronize_theme_metrics() {
        const font_metrics font =
            font_t::stock(font_role::control).get_metrics();
        _row_height = std::max(20, font.height + 4);
        try {
            wnd *root = this;
            while (root->get_parent())
                root = root->get_parent();
            gpx &graphics = root->get_gpx();
            auto painter = theme::create(graphics);
            const theme::metrics values = painter->defaults();
            apply_theme_metrics(values);
        } catch (const std::runtime_error &) {
            // Early native creation may not yet expose a drawable.
        }
        _scroll_offset = std::min(_scroll_offset,
                                  maximum_scroll_offset());
        ensure_item_visible(_selected_item);
    }

    void tree_view::apply_theme_metrics(
        const theme::metrics &values) {
        _row_height = std::max(
            {1,
             values.tree_row_height > 0
                 ? values.tree_row_height
                 : values.list_item_height,
             static_cast<int>(_icon_size.h) +
                 std::max(0, values.tree_icon_vertical_padding)});
        _disclosure_size = std::max(1, values.disclosure_size);
        if (!_lines_visible_explicit)
            _lines_visible = values.tree_lines_visible;
        _horizontal_padding = std::max(
            0,
            values.tree_horizontal_padding >= 0
                ? values.tree_horizontal_padding
                : values.header_padding_x);
        _item_gap = std::max(
            0,
            values.tree_item_gap >= 0
                ? values.tree_item_gap
                : values.header_gap);
        _indent_width = values.tree_indent_width > 0
                            ? values.tree_indent_width
                            : std::max(
                                  _disclosure_size + _item_gap,
                                  values.header_padding_x * 2 +
                                      _disclosure_size);
        _scroll_offset = std::min(_scroll_offset,
                                  maximum_scroll_offset());
        ensure_item_visible(_selected_item);
    }

    void tree_view::draw_background(
        gpx &,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_surface(bounds, surface_kind::content, state);
    }

    void tree_view::draw_row_background(
        gpx &,
        theme &appearance,
        const tree_view_visible_item &,
        const tree_view_item &,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_selection(bounds, selection_shape::row, state);
    }

    void tree_view::draw_connectors(
        gpx &graphics,
        theme &appearance,
        const tree_view_visible_item &visible,
        const tree_view_item &item,
        const rect &row_bounds,
        const rect &disclosure_bounds,
        const theme::state &) {
        if (!_lines_visible)
            return;
        const theme::palette colors = appearance.native_palette();
        const int center_x = disclosure_bounds.p.x +
                             disclosure_bounds.d.w / 2;
        const int center_y = row_bounds.p.y + row_bounds.d.h / 2;
        graphics.set_ink(colors.separator).set_pen(1);
        if (visible.depth > 0) {
            graphics.draw_line(
                point(static_cast<coord>(
                          center_x - disclosure_bounds.d.w),
                      static_cast<coord>(center_y)),
                point(static_cast<coord>(
                          disclosure_bounds.p.x - 1),
                      static_cast<coord>(center_y)));
        }
        if (item.expanded && !item.children.empty()) {
            graphics.draw_line(
                point(static_cast<coord>(center_x),
                      static_cast<coord>(disclosure_bounds.y2())),
                point(static_cast<coord>(center_x),
                      static_cast<coord>(row_bounds.y2())));
        }
    }

    void tree_view::draw_disclosure(
        gpx &,
        theme &appearance,
        const tree_view_visible_item &,
        const tree_view_item &item,
        const rect &bounds,
        const theme::state &state) {
        if (!item.children.empty()) {
            appearance.draw_disclosure(
                bounds,
                item.expanded ? disclosure_state::expanded
                              : disclosure_state::collapsed,
                state);
        }
    }

    void tree_view::draw_item_image(
        gpx &graphics,
        theme &,
        const tree_view_visible_item &,
        const tree_view_item &item,
        const rect &bounds,
        const theme::state &) {
        if (item.image)
            graphics.draw_img(
                *item.image, bounds, image_filter::linear);
    }

    void tree_view::draw_item_text(
        gpx &graphics,
        theme &appearance,
        const tree_view_visible_item &,
        const tree_view_item &item,
        const rect &bounds,
        const theme::state &state) {
        const theme::palette colors = appearance.native_palette();
        graphics.set_font(font_t::stock(font_role::control))
            .set_ink(
                state.disabled
                    ? colors.selection_inactive_text
                    : (state.selected ? colors.selection_text
                                      : colors.content_text))
            .draw_text(
                item.text,
                bounds,
                text_layout{text_align::start,
                            text_valign::center,
                            text_overflow::ellipsis,
                            false});
    }

    void tree_view::draw_row_focus(
        gpx &,
        theme &appearance,
        const tree_view_visible_item &,
        const tree_view_item &,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_focus(bounds, state);
    }

    void tree_view::draw_scrollbar(
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
