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

#include "collection_view.h"
#include "graphics.h"

namespace native
{
    class icon_view;

    namespace detail
    {
        class control_render_access;
        void draw_icon_view(icon_view &control, gpx &graphics);
    }

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
    class icon_view : public collection_view
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

        // Append one item with builder syntax.
        icon_view &operator<<(icon_view_item item);

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

        // Set and clamp the vertical content offset.
        icon_view &set_scroll_offset(int offset) override;

        // Return one client-relative item rectangle.
        rect get_item_bounds(std::size_t index) const;

        // Return the item at a client point, or -1 for empty space.
        int item_at(point position) const;

        // Return the complete unscrolled grid dimensions.
        size get_content_dimensions() const;

        // Cache a native user selection and emit once when it changes.
        virtual void on_native_selection(int index);

        // Emit activation for a valid enabled item.
        virtual void on_native_activate(int index);

        // Apply one backend-originated spatial navigation command.
        virtual void on_native_navigation(
            icon_view_navigation navigation);

        // Scroll vertically by a signed pixel delta.
        virtual void on_native_scroll(int delta);

    protected:
        // Create the backend icon-view resource.
        void create_native() override;

        // Destroy the backend icon-view resource.
        void destroy_native() override;

        // Show the backend icon-view resource.
        void show_native() override;

    public:

        // Emits the selected index after a user-originated change.
        signal<int> on_selection_change;

        // Emits an enabled item index after user activation.
        signal<int> on_item_activate;

    protected:
        // Clamp scrolling and refresh layout after a resize.
        void on_bounds_changed() override;

        // Apply cached item values to the created native control.
        virtual void apply_items();

        // Apply cached image dimensions to the native control.
        virtual void apply_icon_size();

        // Apply cached label placement to the native control.
        virtual void apply_label_mode();

        // Apply cached selection to the native control.
        virtual void apply_selected_index();

        // Apply cached scrolling to the native control.
        void apply_scroll_offset() override;

        // Refresh dimensions from the current native theme.
        void synchronize_theme_metrics() override;

        // Draw the complete icon-view background and frame.
        virtual void draw_background(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        // Draw one item's background and selection.
        virtual void draw_item_background(
            gpx &graphics,
            theme &appearance,
            std::size_t index,
            const icon_view_item &item,
            const rect &bounds,
            const theme::state &state);

        // Draw one item's optional image inside its image box.
        virtual void draw_item_image(
            gpx &graphics,
            theme &appearance,
            std::size_t index,
            const icon_view_item &item,
            const rect &bounds,
            const theme::state &state);

        // Draw one item's label inside its label box.
        virtual void draw_item_label(
            gpx &graphics,
            theme &appearance,
            std::size_t index,
            const icon_view_item &item,
            const rect &bounds,
            const theme::state &state);

        // Draw focus around one completed item.
        virtual void draw_item_focus(
            gpx &graphics,
            theme &appearance,
            std::size_t index,
            const icon_view_item &item,
            const rect &bounds,
            const theme::state &state);

        // Draw one complete icon-view scrollbar.
        virtual void draw_scrollbar(
            gpx &graphics,
            theme &appearance,
            scrollbar_orientation orientation,
            const rect &track,
            const rect &thumb,
            const theme::state &state);

    private:
        friend class detail::control_render_access;
        friend void detail::draw_icon_view(
            icon_view &control, gpx &graphics);

        std::vector<icon_view_item> _items;
        size _icon_size = {48, 48};
        icon_view_label_mode _label_mode =
            icon_view_label_mode::below;
        int _selected_index = -1;
        int _item_padding = 6;
        int _item_gap = 4;
        int _label_gap = 4;
        int _label_height = 20;
        int _minimum_item_width = 80;

        void validate_index(int index) const;
        int column_count() const;
        int item_width() const;
        int item_height() const;
        int maximum_scroll_offset() const override;
        int navigated_index(icon_view_navigation navigation) const;
        void ensure_selection_visible();
    };
} // namespace native
