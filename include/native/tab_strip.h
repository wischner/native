//
// Declares a themed non-client strip of labeled tabs. Unlike tab_view it
// owns no pages: the tabs choose what one content window shows, which is
// what a spreadsheet's sheet tabs or a browser's page tabs need.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "non_client.h"
#include "signal.h"
#include "theme.h"

namespace native
{
    // Presents selectable labels along one edge of any window.
    class tab_strip : public non_client
    {
    public:
        // Attach a strip at an edge; extent 0 selects the theme's tab height
        // once the owner is created.
        explicit tab_strip(wnd &owner,
                           window_edge edge = window_edge::bottom,
                           int extent = 24);

        // Return the labels in display order.
        const std::vector<std::string> &get_items() const;

        // Replace the labels silently, keeping the selection in range.
        tab_strip &set_items(std::vector<std::string> items);

        // Return the selected index, or -1 when the strip is empty.
        int get_selected_index() const;

        // Select a tab silently and scroll it into view.
        tab_strip &set_selected_index(int index);

        // Return a tab's strip-relative rectangle, empty when out of view.
        rect get_tab_bounds(std::size_t index) const;

        // Return the index of the first tab in view.
        std::size_t get_first_visible() const;

        // Scroll tabs so one index comes first.
        tab_strip &set_first_visible(std::size_t index);

        // Emits the index after a user-originated selection change.
        signal<int> on_selection_change;

        // Emits the index of a double-clicked tab.
        signal<int> on_activate;

    protected:
        // Paint the strip and every visible tab.
        void draw(gpx &graphics, const rect &bounds) override;

        // Select the tab under a press.
        bool pointer_click(const mouse_event &event) override;

        // Draw the strip behind the tabs.
        virtual void draw_background(gpx &graphics, theme &appearance,
                                     const rect &bounds);

        // Draw one tab; state.selected marks the selected one.
        virtual void draw_tab(gpx &graphics, theme &appearance,
                              const rect &bounds, const std::string &label,
                              const theme::state &state);

    private:
        // Return the width of a tab's label and padding.
        int tab_length(std::size_t index) const;

        // Return the tab under a host-relative point, or -1.
        int tab_at(const point &position) const;

        std::vector<std::string> _items;
        int _selected = -1;
        std::size_t _first = 0;
    };
}
