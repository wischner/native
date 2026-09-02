//
// Declares portable split, tab, floating-pane, and docking-host state.
// The host coordinates an existing window while the layout manager
// arranges borrowed pane windows.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "layout.h"
#include "signal.h"
#include "theme.h"

namespace native
{
    class app_wnd;

    // Stable application identity for one dockable pane.
    using dock_pane_id = std::uint64_t;

    // Stable identity for one split or tab node in a saved layout.
    using dock_node_id = std::uint64_t;

    // Selects a pane's placement relative to another docked pane.
    enum class dock_position
    {
        center,
        left,
        right,
        top,
        bottom
    };

    // Selects the direction in which a split's children are arranged.
    enum class dock_orientation
    {
        horizontal,
        vertical
    };

    // Identifies the portable kind of one layout-tree node.
    enum class dock_node_kind
    {
        tabs,
        split
    };

    // Identifies where a registered pane currently lives.
    enum class dock_pane_location
    {
        docked,
        floating,
        auto_hidden,
        hidden
    };

    // Describes one borrowed pane and its persistent application ID.
    struct dock_pane
    {
        dock_pane_id id = 0;
        std::string title;
        wnd *content = nullptr;
        size minimum_size = size(80, 60);
        bool closable = true;
        bool floatable = true;
        bool pinnable = true;

        // Construct an empty, invalid pane descriptor.
        dock_pane();

        // Construct a pane borrowing its content window.
        dock_pane(dock_pane_id pane_id,
                  std::string pane_title,
                  wnd &pane_content);
    };

    // Stores one copyable node of a persistent docking layout tree.
    struct dock_layout_node
    {
        dock_node_id id = 0;
        dock_node_kind kind = dock_node_kind::tabs;
        dock_orientation orientation = dock_orientation::horizontal;
        float split_ratio = 0.5f;
        std::vector<dock_layout_node> children;
        std::vector<dock_pane_id> panes;
        dock_pane_id active_pane = 0;
    };

    // Stores the bounds of one independently floating pane.
    struct dock_floating_pane
    {
        dock_pane_id pane = 0;
        rect bounds;
    };

    // Stores one pane collapsed into an edge auto-hide strip.
    struct dock_auto_hide_pane
    {
        dock_pane_id pane = 0;
        dock_position edge = dock_position::left;
    };

    // Stores a complete, backend-neutral, versioned docking layout.
    struct dock_layout_state
    {
        std::uint32_t version = 2;
        std::optional<dock_layout_node> root;
        std::vector<dock_floating_pane> floating;
        std::vector<dock_pane_id> hidden;
        std::vector<dock_auto_hide_pane> auto_hidden;
    };

    // Stores the hit-test geometry of one visible dock tab.
    struct dock_tab_region
    {
        dock_pane_id pane = 0;
        rect bounds;
        rect pin_bounds;
        rect close_bounds;
    };

    // Stores hit-test geometry for one collapsed edge tab.
    struct dock_auto_hide_region
    {
        dock_pane_id pane = 0;
        dock_position edge = dock_position::left;
        rect bounds;
    };

    // Stores resolved geometry for one visible split or tab node.
    struct dock_layout_region
    {
        dock_node_id node = 0;
        dock_node_kind kind = dock_node_kind::tabs;
        rect bounds;
        rect tab_strip;
        rect content;
        rect splitter;
        std::vector<dock_tab_region> tabs;
    };

    // Identifies one user-originated change to a docking host.
    enum class dock_action
    {
        activated,
        docked,
        floated,
        closed,
        shown,
        tab_reordered,
        split_resized,
        auto_hidden,
        pinned,
        auto_hide_revealed,
        auto_hide_collapsed,
        layout_restored
    };

    // Describes one user-originated docking action.
    struct dock_event
    {
        dock_action action = dock_action::activated;
        dock_pane_id pane = 0;
        dock_node_id node = 0;
        dock_position position = dock_position::center;
    };

    // Serialize a docking layout to the current Native Dock format.
    std::string serialize_dock_layout(const dock_layout_state &state);

    // Parse a complete Native Dock layout, including legacy version 1.
    dock_layout_state deserialize_dock_layout(std::string_view text);

    // Arranges active docked pane windows from a split-and-tab tree.
    class dock_layout_manager final : public layout_manager
    {
    public:
        // Construct an empty docking layout.
        dock_layout_manager();

        // Release the portable tree and borrowed child registrations.
        ~dock_layout_manager() override;

        // Docking layouts own state and cannot be copied.
        dock_layout_manager(const dock_layout_manager &) = delete;
        dock_layout_manager &
        operator=(const dock_layout_manager &) = delete;

        // Register and initially dock one unique non-zero pane.
        dock_layout_manager &add_pane(
            const dock_pane &pane,
            dock_position position = dock_position::center,
            dock_pane_id relative_to = 0);

        // Permanently unregister one pane without owning its window.
        dock_layout_manager &remove_pane(dock_pane_id pane);

        // Return a registered pane descriptor, or null when absent.
        const dock_pane *get_pane(dock_pane_id pane) const;

        // Return every registered pane in registration order.
        const std::vector<dock_pane> &get_panes() const;

        // Return the current location of a registered pane.
        dock_pane_location get_pane_location(
            dock_pane_id pane) const;

        // Place a pane beside or in the tabs of another pane.
        dock_layout_manager &dock(
            dock_pane_id pane,
            dock_position position = dock_position::center,
            dock_pane_id relative_to = 0);

        // Move a floatable pane to independent screen bounds.
        dock_layout_manager &float_pane(dock_pane_id pane,
                                        const rect &bounds);

        // Collapse a pinnable pane into one host edge.
        dock_layout_manager &auto_hide_pane(
            dock_pane_id pane,
            dock_position edge = dock_position::left);

        // Restore an auto-hidden pane to the dock tree.
        dock_layout_manager &pin_pane(
            dock_pane_id pane,
            dock_position position = dock_position::center,
            dock_pane_id relative_to = 0);

        // Remove a pane from visible docked or floating layout state.
        dock_layout_manager &hide_pane(dock_pane_id pane);

        // Reveal a hidden pane in a docked location.
        dock_layout_manager &show_pane(
            dock_pane_id pane,
            dock_position position = dock_position::center,
            dock_pane_id relative_to = 0);

        // Select a pane inside its current tab group.
        dock_layout_manager &activate_pane(dock_pane_id pane);

        // Reorder a pane before a sibling, or append it when before is 0.
        dock_layout_manager &move_tab(dock_pane_id pane,
                                      dock_pane_id before = 0);

        // Change one split ratio, clamped by child minimum dimensions.
        dock_layout_manager &set_split_ratio(dock_node_id node,
                                             float ratio);

        // Return the complete copyable portable layout state.
        dock_layout_state get_state() const;

        // Restore known IDs and dock newly registered panes by default.
        dock_layout_manager &set_state(const dock_layout_state &state);

        // Return resolved visible regions from the latest layout pass.
        const std::vector<dock_layout_region> &get_regions() const;

        // Return collapsed edge-tab geometry from the latest layout.
        const std::vector<dock_auto_hide_region> &
        get_auto_hide_regions() const;

        // Set native-theme-derived tab and splitter dimensions.
        dock_layout_manager &set_metrics(int tab_height,
                                         int splitter_extent,
                                         int tab_padding);

        // Arrange active docked children within parent-relative bounds.
        void relayout(wnd *parent, const rect &bounds) override;

        // Register a direct child without changing pane identity.
        void add_child(wnd *child) override;

        // Remove a direct child without unregistering its pane.
        void remove_child(wnd *child) override;

        // Return direct children currently parented to the host surface.
        const std::vector<wnd *> &children() const override;

    private:
        class implementation;
        std::unique_ptr<implementation> _impl;

        friend class dock_host;
    };

    // Coordinates painting, input, floating shells, and pane lifecycle.
    class dock_host
    {
    public:
        // Use an application window as owner and docking surface.
        explicit dock_host(app_wnd &surface);

        // Coordinate a docking surface using an application-window owner.
        dock_host(app_wnd &owner, wnd &surface);

        // Destroy floating shells and detach from the borrowed surface.
        virtual ~dock_host();

        // A host has signal connections and cannot be copied or moved.
        dock_host(const dock_host &) = delete;
        dock_host &operator=(const dock_host &) = delete;
        dock_host(dock_host &&) = delete;
        dock_host &operator=(dock_host &&) = delete;

        // Return the borrowed surface being coordinated.
        wnd &get_surface() const;

        // Return the geometry manager installed on the surface.
        dock_layout_manager &get_layout() const;

        // Register and initially dock one pane.
        dock_host &add_pane(
            const dock_pane &pane,
            dock_position position = dock_position::center,
            dock_pane_id relative_to = 0);

        // Permanently unregister a pane and detach its content window.
        dock_host &remove_pane(dock_pane_id pane);

        // Programmatically dock a pane without emitting on_change.
        dock_host &dock(
            dock_pane_id pane,
            dock_position position = dock_position::center,
            dock_pane_id relative_to = 0);

        // Programmatically float a pane without emitting on_change.
        dock_host &float_pane(dock_pane_id pane,
                              const rect &bounds);

        // Programmatically collapse a pane into an edge strip.
        dock_host &auto_hide_pane(
            dock_pane_id pane,
            dock_position edge = dock_position::left);

        // Programmatically pin an auto-hidden pane back into the tree.
        dock_host &pin_pane(
            dock_pane_id pane,
            dock_position position = dock_position::center,
            dock_pane_id relative_to = 0);

        // Reveal one collapsed pane until it is collapsed or pinned.
        dock_host &reveal_auto_hide(dock_pane_id pane);

        // Collapse the currently revealed auto-hidden pane, if any.
        dock_host &collapse_auto_hide();

        // Return the currently revealed pane, or zero when collapsed.
        dock_pane_id get_revealed_auto_hide() const;

        // Programmatically close a closable pane into hidden state.
        dock_host &close_pane(dock_pane_id pane);

        // Programmatically reveal a hidden pane.
        dock_host &show_pane(
            dock_pane_id pane,
            dock_position position = dock_position::center,
            dock_pane_id relative_to = 0);

        // Programmatically select a pane in its tab group.
        dock_host &activate_pane(dock_pane_id pane);

        // Programmatically change a tab's sibling order.
        dock_host &move_tab(dock_pane_id pane,
                            dock_pane_id before = 0);

        // Programmatically resize a split.
        dock_host &set_split_ratio(dock_node_id node, float ratio);

        // Return a complete copyable snapshot of the current layout.
        dock_layout_state get_layout_state() const;

        // Restore a typed layout without emitting user-action signals.
        dock_host &set_layout_state(const dock_layout_state &state);

        // Serialize the current layout to the Native Dock format.
        std::string serialize_layout() const;

        // Parse and restore a Native Dock layout, including version 1.
        dock_host &restore_layout(std::string_view text);

        // Notify one accepted user-originated docking action.
        virtual void on_native_change(const dock_event &event);

        // Emits exactly once for each accepted user docking action.
        signal<dock_event> on_change;

    protected:
        // Draw one docked or floating pane tab.
        virtual void draw_tab(
            gpx &graphics,
            theme &appearance,
            const dock_pane &pane,
            const dock_tab_region &tab,
            bool selected,
            bool hot,
            bool close_pressed,
            bool pin_pressed);

        // Draw one docked content or tab-strip surface.
        virtual void draw_surface(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            surface_kind kind,
            const theme::state &state);

        // Draw one splitter including its native separator.
        virtual void draw_splitter(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            dock_orientation orientation,
            const theme::state &state);

        // Draw one collapsed edge tab.
        virtual void draw_auto_hide_tab(
            gpx &graphics,
            theme &appearance,
            const dock_pane &pane,
            const dock_auto_hide_region &region,
            const theme::state &state);

        // Draw the compact caption of a revealed auto-hide pane.
        virtual void draw_auto_hide_caption(
            gpx &graphics,
            theme &appearance,
            const dock_pane &pane,
            const rect &caption,
            const rect &pin,
            const rect &close,
            bool pin_pressed,
            bool close_pressed);

        // Draw one active drop-target preview.
        virtual void draw_drop_preview(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        // Draw the compact operation-and-pane label shown during a drag.
        virtual void draw_drop_destination(
            gpx &graphics,
            theme &appearance,
            const dock_pane &pane,
            dock_position position,
            const rect &bounds,
            const theme::state &state);

        // Draw one target in the drag-time docking compass.
        virtual void draw_drop_guide(
            gpx &graphics,
            theme &appearance,
            dock_position position,
            const rect &bounds,
            const theme::state &state);

    private:
        class implementation;
        std::unique_ptr<implementation> _impl;

        friend class implementation;
    };
} // namespace native
