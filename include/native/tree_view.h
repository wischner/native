//
// Declares a portable single-selection hierarchical tree control.
// Items own their descendant values, retain shared images, and expose
// stable identity independently of native widget rows and handles.
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
    class tree_view;

    namespace detail
    {
        class control_render_access;
        void draw_tree_view(tree_view &control, gpx &graphics);
    }

    // Identifies one tree item independently of its display position.
    using tree_item_id = std::uint64_t;

    // Represents the absence of a tree item or selection.
    inline constexpr tree_item_id invalid_tree_item_id = 0;

    // Stores one owned tree branch and its portable presentation state.
    struct tree_view_item
    {
        // Construct an empty item for subsequent property assignment.
        tree_view_item();

        // Construct one branch from text, image, ID, children, and state.
        tree_view_item(
            std::string text,
            std::shared_ptr<const img> image,
            tree_item_id id,
            std::vector<tree_view_item> children = {},
            bool expanded = false,
            bool enabled = true);

        std::string text;
        std::shared_ptr<const img> image;
        tree_item_id id = invalid_tree_item_id;
        std::vector<tree_view_item> children;
        bool expanded = false;
        bool enabled = true;
    };

    // Construct one text-only branch for builder-style appending.
    tree_view_item tree_node(
        std::string text,
        tree_item_id id,
        std::vector<tree_view_item> children = {},
        bool expanded = false,
        bool enabled = true);

    // Identifies one classic tree keyboard command.
    enum class tree_view_navigation
    {
        up,
        down,
        left,
        right,
        home,
        end,
        page_up,
        page_down,
        toggle,
        activate
    };

    // Selects the native tree's visual presentation. Backends without a
    // distinct three-dimensional outline retain their normal presentation.
    enum class tree_view_presentation
    {
        native,
        three_dimensional
    };

    // Identifies the semantic part under a tree pointer position.
    enum class tree_view_hit_part
    {
        none,
        row,
        disclosure
    };

    // Describes one client-relative tree pointer hit.
    struct tree_view_hit
    {
        tree_item_id id = invalid_tree_item_id;
        tree_view_hit_part part = tree_view_hit_part::none;
    };

    // Describes one item in the current flattened visible hierarchy.
    struct tree_view_visible_item
    {
        tree_item_id id = invalid_tree_item_id;
        std::size_t depth = 0;
    };

    // Presents a classic expandable hierarchy with stable-ID selection.
    class tree_view : public collection_view
    {
    public:
        // Construct a tree from owned branches and scalar bounds.
        tree_view(std::vector<tree_view_item> items = {},
                  coord x = 0,
                  coord y = 0,
                  dim width = 240,
                  dim height = 240);

        // Construct a tree from owned branches, position, and dimensions.
        tree_view(const std::vector<tree_view_item> &items,
                  const point &position,
                  const size &dimensions);

        // Construct a tree from owned branches and complete bounds.
        tree_view(const std::vector<tree_view_item> &items,
                  const rect &bounds);

        // Destroy the control and its native resource if it exists.
        ~tree_view() override;

        // Return the owned root branches in logical order.
        const std::vector<tree_view_item> &get_items() const;

        // Replace every branch after validating unique non-zero IDs.
        tree_view &set_items(std::vector<tree_view_item> items);

        // Append a root or child branch and update a created control.
        tree_view &add_item(
            tree_view_item item,
            tree_item_id parent_id = invalid_tree_item_id);

        // Append one root branch with builder syntax.
        tree_view &operator<<(tree_view_item item);

        // Remove one item and all descendants by stable ID.
        tree_view &remove_item(tree_item_id id);

        // Remove every branch and clear selection and scrolling.
        tree_view &clear_items();

        // Return an item by stable ID or throw std::out_of_range.
        const tree_view_item &get_item(tree_item_id id) const;

        // Return whether an item with the stable ID exists.
        bool contains_item(tree_item_id id) const;

        // Return the selected stable ID, or invalid_tree_item_id.
        tree_item_id get_selected_item() const;

        // Select an enabled item, or clear selection, without a signal.
        tree_view &set_selected_item(tree_item_id id);

        // Expand or collapse a branch without emitting an action signal.
        tree_view &set_expanded(tree_item_id id, bool expanded);

        // Return whether one branch is currently expanded.
        bool get_expanded(tree_item_id id) const;

        // Expand every ancestor and scroll enough to reveal an item.
        tree_view &reveal_item(tree_item_id id);

        // Set non-zero logical dimensions used for optional item images.
        tree_view &set_icon_size(size dimensions);

        // Return the logical dimensions used for optional item images.
        size get_icon_size() const;

        // Enable or disable classic hierarchy connector lines.
        tree_view &set_lines_visible(bool visible);

        // Return whether classic hierarchy connector lines are visible.
        bool get_lines_visible() const;

        // Show or hide the complete outer tree border.
        tree_view &set_border_visible(bool visible);

        // Return whether the complete outer border is visible.
        bool get_border_visible() const;

        // Select the platform-native or optional three-dimensional outline.
        tree_view &set_presentation(tree_view_presentation presentation);

        // Return the requested tree presentation.
        tree_view_presentation get_presentation() const;

        // Return the current number of flattened visible items.
        std::size_t get_visible_item_count() const;

        // Return one flattened item and depth or throw std::out_of_range.
        tree_view_visible_item get_visible_item(
            std::size_t index) const;

        // Return one visible row's client-relative bounds.
        rect get_row_bounds(std::size_t index) const;

        // Return one visible row's disclosure-indicator bounds.
        rect get_disclosure_bounds(std::size_t index) const;

        // Return the semantic tree part under a client position.
        tree_view_hit hit_test(point position) const;

        // Return the item at a client point, or invalid_tree_item_id.
        tree_item_id item_at(point position) const;

        // Set and clamp the vertical content offset without a signal.
        tree_view &set_scroll_offset(int offset) override;

        // Cache a backend user selection and emit once when it changes.
        virtual void on_native_selection(tree_item_id id);

        // Cache a backend disclosure action and emit once when changed.
        virtual void on_native_expansion(tree_item_id id,
                                         bool expanded);

        // Apply selection, expansion, activation, or navigation input.
        virtual void on_native_navigation(
            tree_view_navigation navigation);

        // Apply classic double-click expansion and item activation.
        virtual void on_native_double_click(tree_item_id id);

        // Emit activation for one valid enabled item.
        virtual void on_native_activate(tree_item_id id);

        // Scroll vertically by a signed pixel delta.
        virtual void on_native_scroll(int delta);

    protected:
        // Create the backend tree resource.
        void create_native() override;

        // Destroy the backend tree resource.
        void destroy_native() override;

        // Show the backend tree resource.
        void show_native() override;

    public:

        // Emits a stable ID after a user-originated selection change.
        signal<tree_item_id> on_selection_change;

        // Emits an enabled stable ID after user activation.
        signal<tree_item_id> on_item_activate;

        // Emits stable ID and state after a user disclosure action.
        signal<tree_item_id, bool> on_expanded_change;

    protected:
        // Clamp scrolling and refresh native geometry after a resize.
        void on_bounds_changed() override;

        // Apply cached hierarchy to the created native control.
        virtual void apply_items();

        // Apply cached selection to the created native control.
        virtual void apply_selection();

        // Apply one cached branch expansion to the native control.
        virtual void apply_expansion(tree_item_id id, bool expanded);

        // Apply cached scrolling to the native control.
        void apply_scroll_offset() override;

        // Refresh dimensions from the current native theme.
        void synchronize_theme_metrics() override;

        // Draw the complete tree background and frame.
        virtual void draw_background(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        // Draw one visible row's background and selection.
        virtual void draw_row_background(
            gpx &graphics,
            theme &appearance,
            const tree_view_visible_item &visible,
            const tree_view_item &item,
            const rect &bounds,
            const theme::state &state);

        // Draw one row's hierarchy connector lines.
        virtual void draw_connectors(
            gpx &graphics,
            theme &appearance,
            const tree_view_visible_item &visible,
            const tree_view_item &item,
            const rect &row_bounds,
            const rect &disclosure_bounds,
            const theme::state &state);

        // Draw one row's native disclosure indicator.
        virtual void draw_disclosure(
            gpx &graphics,
            theme &appearance,
            const tree_view_visible_item &visible,
            const tree_view_item &item,
            const rect &bounds,
            const theme::state &state);

        // Draw one row's optional image.
        virtual void draw_item_image(
            gpx &graphics,
            theme &appearance,
            const tree_view_visible_item &visible,
            const tree_view_item &item,
            const rect &bounds,
            const theme::state &state);

        // Draw one row's text content.
        virtual void draw_item_text(
            gpx &graphics,
            theme &appearance,
            const tree_view_visible_item &visible,
            const tree_view_item &item,
            const rect &bounds,
            const theme::state &state);

        // Draw focus around one completed row.
        virtual void draw_row_focus(
            gpx &graphics,
            theme &appearance,
            const tree_view_visible_item &visible,
            const tree_view_item &item,
            const rect &bounds,
            const theme::state &state);

        // Draw one complete tree scrollbar.
        virtual void draw_scrollbar(
            gpx &graphics,
            theme &appearance,
            scrollbar_orientation orientation,
            const rect &track,
            const rect &thumb,
            const theme::state &state);

        // Draw the complete tree border after rows and scrollbars.
        virtual void draw_border(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

    private:
        friend class detail::control_render_access;

        friend void detail::draw_tree_view(
            tree_view &control, gpx &graphics);

        std::vector<tree_view_item> _items;
        tree_item_id _selected_item = invalid_tree_item_id;
        size _icon_size = {16, 16};
        int _row_height = 20;
        int _indent_width = 18;
        int _disclosure_size = 12;
        int _horizontal_padding = 4;
        int _item_gap = 4;
        bool _lines_visible = false;
        bool _lines_visible_explicit = false;
        bool _border_visible = true;
        tree_view_presentation _presentation =
            tree_view_presentation::native;

        tree_view_item *find_item(tree_item_id id);
        const tree_view_item *find_item(tree_item_id id) const;
        tree_view_item *find_parent(tree_item_id id);
        const tree_view_item *find_parent(tree_item_id id) const;
        std::vector<tree_view_visible_item> visible_items() const;
        void validate_items(
            const std::vector<tree_view_item> &items) const;
        void validate_id(tree_item_id id, bool allow_none) const;
        void apply_theme_metrics(const theme::metrics &values);
        int maximum_scroll_offset() const override;
        void ensure_item_visible(tree_item_id id);
    };
} // namespace native
