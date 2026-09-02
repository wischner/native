//
// Declares a portable native tab view with borrowed page windows.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "theme.h"
#include "wnd.h"

namespace native
{
    class tab_view;

    // Stores one tab label and its borrowed page window.
    class tab_item final
    {
    public:
        // Return the UTF-8 tab label.
        const std::string &get_title() const;

        // Change the label without emitting a selection event.
        tab_item &set_title(std::string title);

        // Return whether this tab can be selected by the user.
        bool get_enabled() const;

        // Enable or disable user selection of this tab.
        tab_item &set_enabled(bool enabled);

        // Return the borrowed page window.
        wnd &get_content() const;

    private:
        friend class tab_view;

        tab_item(tab_view &owner, std::string title, wnd &content);

        tab_view *_owner;
        std::string _title;
        wnd *_content;
        bool _enabled = true;
    };

    // Presents one selected page beneath a row of native tabs.
    class tab_view : public wnd
    {
    public:
        // Construct an empty tab view from scalar bounds.
        tab_view(coord x = 0,
                 coord y = 0,
                 dim width = 320,
                 dim height = 240);

        // Construct an empty tab view from position and dimensions.
        tab_view(const point &position, const size &dimensions);

        // Construct an empty tab view from complete bounds.
        explicit tab_view(const rect &bounds);

        // Destroy the native tab view and detach every borrowed page.
        ~tab_view() override;

        // Return the number of tabs in display order.
        std::size_t get_item_count() const;

        // Return one tab or throw std::out_of_range.
        tab_item &get_item(std::size_t index) const;

        // Append a tab and borrow its uncreated page window.
        tab_item &add_item(const std::string &title, wnd &content);

        // Remove one tab and detach its page window.
        tab_view &remove_item(std::size_t index);

        // Remove all tabs and detach every page window.
        tab_view &clear_items();

        // Return the selected index, or -1 when the view is empty.
        int get_selected_index() const;

        // Select a tab programmatically without emitting an event.
        tab_view &set_selected_index(int index);

        // Return the client-relative bounds occupied by one tab.
        rect get_tab_bounds(std::size_t index) const;

        // Return the client-relative bounds occupied by the page.
        rect get_content_bounds() const;

        // Accept a backend-originated user selection.
        virtual void on_native_selection(int index);

        // Create the backend tab-view resource.
        void create() const override;

        // Destroy the backend tab-view resource.
        void destroy() const override;

        // Show the backend tab-view resource.
        void show() const override;

        // Emits the selected index after a user-originated change.
        signal<int> on_selection_change;

    protected:
        // Recalculate the selected page after a resize.
        void on_bounds_changed() override;

        // Rebuild backend tab labels after the item model changes.
        virtual void apply_items();

        // Apply the cached selection to the backend control.
        virtual void apply_selected_index();

        // Refresh native metrics, pages, and painting.
        virtual void refresh();

        // Refresh the tab height from the active native control.
        virtual void synchronize_theme_metrics();

        // Draw the complete tab-view background.
        virtual void draw_background(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        // Draw one tab surface.
        virtual void draw_tab_background(
            gpx &graphics,
            theme &appearance,
            std::size_t index,
            const tab_item &item,
            const rect &bounds,
            const theme::state &state);

        // Draw one tab label.
        virtual void draw_tab_text(
            gpx &graphics,
            theme &appearance,
            std::size_t index,
            const tab_item &item,
            const rect &bounds,
            const theme::state &state);

        // Draw one tab's focus and edge treatment.
        virtual void draw_tab_border(
            gpx &graphics,
            theme &appearance,
            std::size_t index,
            const tab_item &item,
            const rect &bounds,
            const theme::state &state);

    private:
        friend class tab_item;

        std::vector<std::unique_ptr<tab_item>> _items;
        int _selected_index = -1;
        int _tab_height = 24;
        bool _content_host_is_page = false;

        void validate_index(int index) const;
        void refresh_contents();
        void detach_item(tab_item &item);
        void draw(gpx &graphics);
        bool handle_click(const mouse_event &event);
    };
} // namespace native
