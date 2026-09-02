//
// Declares the portable single-selection list control.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "theme.h"
#include "wnd.h"

namespace native
{
    namespace detail
    {
        class control_render_access;
    }

    class list : public wnd
    {
    public:
        // Construct a list from items and scalar bounds.
        list(std::vector<std::string> items = {},
             coord x = 0,
             coord y = 0,
             dim width = 160,
             dim height = 120);

        // Construct a list from items, a position, and dimensions.
        list(const std::vector<std::string> &items,
             const point &position,
             const size &dimensions);

        // Construct a list from items and complete bounds.
        list(const std::vector<std::string> &items, const rect &bounds);

        // Destroy the control and its native resource if it exists.
        ~list() override;

        // Return the cached items in display order.
        const std::vector<std::string> &get_items() const;

        // Replace all items and update a created native control.
        list &set_items(std::vector<std::string> items);

        // Append one item and update a created native control.
        list &add_item(const std::string &item);

        // Remove the item at index or throw std::out_of_range.
        list &remove_item(std::size_t index);

        // Remove all items and clear the selection.
        list &clear_items();

        // Return the selected item index, or -1 when none is selected.
        int get_selected_index() const;

        // Select an index, or -1, without emitting an action signal.
        list &set_selected_index(int index);

        // Cache a native user selection and emit on_selection_change.
        virtual void on_native_selection(int index);

        // Create the backend list resource.
        void create() const override;

        // Destroy the backend list resource.
        void destroy() const override;

        // Show the backend list resource.
        void show() const override;

        // Emits the selected index after a user-originated change.
        signal<int> on_selection_change;

    protected:
        // Draw the complete list using the active native theme.
        virtual void draw_control(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        // Apply all cached items to the created native control.
        virtual void apply_items();

        // Apply the cached selection to the created native control.
        virtual void apply_selected_index();

    private:
        friend class detail::control_render_access;

        std::vector<std::string> _items;
        int _selected_index = -1;
        void validate_index(int index) const;
    };
} // namespace native
