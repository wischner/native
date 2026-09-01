//
// Declares a portable single-selection image and label collection.
// Items wrap spatially, retain shared image ownership, and expose
// backend-neutral selection, activation, navigation, and scrolling.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "graphics.h"
#include "wnd.h"

namespace native
{
    // Selects where an icon-view label appears relative to its image.
    enum class icon_view_label_mode
    {
        below,
        beside,
        hidden
    };

    // Identifies a portable spatial-navigation command.
    enum class icon_view_navigation
    {
        left,
        right,
        up,
        down,
        home,
        end,
        page_up,
        page_down
    };

    // Stores one image, UTF-8 label, stable application ID, and state.
    struct icon_view_item
    {
        std::string text;
        std::shared_ptr<const img> image;
        std::uint64_t id = 0;
        bool enabled = true;
    };

    // Presents image-and-label items in a wrapping, scrolling grid.
    class icon_view : public wnd
    {
    public:
        // Construct an icon view from items and scalar bounds.
        icon_view(std::vector<icon_view_item> items = {},
                  coord x = 0,
                  coord y = 0,
                  dim width = 240,
                  dim height = 200);

        // Construct an icon view from position and dimensions.
        icon_view(const std::vector<icon_view_item> &items,
                  const point &position,
                  const size &dimensions);

        // Construct an icon view from items and complete bounds.
        icon_view(const std::vector<icon_view_item> &items,
                  const rect &bounds);

        // Destroy the control and its native resource if it exists.
        ~icon_view() override;

        // Return the cached items in display order.
        const std::vector<icon_view_item> &get_items() const;

        // Replace every item and update a created native control.
        icon_view &set_items(std::vector<icon_view_item> items);

        // Append one item and update a created native control.
        icon_view &add_item(icon_view_item item);

        // Remove an item by index or throw std::out_of_range.
        icon_view &remove_item(std::size_t index);

        // Remove all items and clear selection and scrolling.
        icon_view &clear_items();

        // Set the non-zero logical image box dimensions.
        icon_view &set_icon_size(size dimensions);

        // Return the logical image box dimensions.
        size get_icon_size() const;

        // Set label placement and update item layout.
        icon_view &set_label_mode(icon_view_label_mode mode);

        // Return the current label placement.
        icon_view_label_mode get_label_mode() const;

        // Return the selected item index, or -1 when none is selected.
        int get_selected_index() const;

        // Select an index, or -1, without emitting an action signal.
        icon_view &set_selected_index(int index);

        // Return the vertical content offset in pixels.
        int get_scroll_offset() const;

        // Set and clamp the vertical content offset.
        icon_view &set_scroll_offset(int offset);

        // Return one client-relative item rectangle.
        rect get_item_bounds(std::size_t index) const;

        // Return the item at a client point, or -1 for empty space.
        int item_at(point position) const;

        // Return the complete unscrolled grid dimensions.
        size get_content_dimensions() const;

        // Cache a native user selection and emit once when it changes.
        void on_native_selection(int index);

        // Emit activation for a valid enabled item.
        void on_native_activate(int index);

        // Apply one backend-originated spatial navigation command.
        void on_native_navigation(icon_view_navigation navigation);

        // Scroll vertically by a signed pixel delta.
        void on_native_scroll(int delta);

        // Return whether the collection currently has keyboard focus.
        bool get_focused() const;

        // Cache backend focus entry or departure without a signal.
        void on_native_focus(bool focused);

        // Create the backend icon-view resource.
        void create() const override;

        // Destroy the backend icon-view resource.
        void destroy() const override;

        // Show the backend icon-view resource.
        void show() const override;

        // Emits the selected index after a user-originated change.
        signal<int> on_selection_change;

        // Emits an enabled item index after user activation.
        signal<int> on_item_activate;

    protected:
        // Clamp scrolling and refresh layout after a resize.
        void on_bounds_changed() override;

    private:
        std::vector<icon_view_item> _items;
        size _icon_size = {48, 48};
        icon_view_label_mode _label_mode =
            icon_view_label_mode::below;
        int _selected_index = -1;
        int _scroll_offset = 0;
        bool _focused = false;
        int _item_padding = 6;
        int _item_gap = 4;
        int _label_gap = 4;
        int _label_height = 20;
        int _minimum_item_width = 80;

        void apply_items();
        void apply_icon_size();
        void apply_label_mode();
        void apply_selected_index();
        void apply_scroll_offset();
        void validate_index(int index) const;
        int column_count() const;
        int item_width() const;
        int item_height() const;
        int maximum_scroll_offset() const;
        int navigated_index(icon_view_navigation navigation) const;
        void ensure_selection_visible();
        void synchronize_theme_metrics();
    };
} // namespace native
