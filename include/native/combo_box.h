//
// Declares the portable editable and selection-only combo box.
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
    namespace detail { class control_render_access; }

    enum class combo_box_style
    {
        drop_down_list,
        editable
    };

    class combo_box : public wnd
    {
    public:
        // Construct a combo box from items, style, and scalar bounds.
        combo_box(std::vector<std::string> items = {},
                  combo_box_style style = combo_box_style::drop_down_list,
                  coord x = 0,
                  coord y = 0,
                  dim width = 160,
                  dim height = 24);

        // Construct a combo box from items, style, position, and size.
        combo_box(const std::vector<std::string> &items,
                  combo_box_style style,
                  const point &position,
                  const size &dimensions);

        // Construct a combo box from items, style, and complete bounds.
        combo_box(const std::vector<std::string> &items,
                  combo_box_style style,
                  const rect &bounds);

        // Destroy the control and its native resource if it exists.
        ~combo_box() override;

        // Return the owned item labels in display order.
        const std::vector<std::string> &get_items() const;

        // Replace every item without emitting user-action signals.
        combo_box &set_items(std::vector<std::string> items);

        // Append one item without changing the current selection.
        combo_box &add_item(const std::string &item);

        // Remove an item by index or throw std::out_of_range.
        combo_box &remove_item(std::size_t index);

        // Remove every item and clear a selection-only value.
        combo_box &clear_items();

        // Return the selected item index, or -1 when none is selected.
        int get_selected_index() const;

        // Select an item by index, or pass -1 to clear selection.
        combo_box &set_selected_index(int index);

        // Return the complete cached text shown by the control.
        const std::string &get_text() const;

        // Set editable text or select the matching selection-only item.
        combo_box &set_text(const std::string &text);

        // Return whether text entry is permitted.
        combo_box_style get_style() const;

        // Change the native combo style without emitting an action signal.
        combo_box &set_style(combo_box_style style);

        // Accept a selection reported by the native widget.
        virtual void on_native_selection(int index);

        // Accept editable text reported by the native widget.
        virtual void on_native_text(const std::string &text);

        // Accept a native popup visibility transition.
        virtual void on_native_drop_down(bool open);

        // Create the backend resource for this control.
        void create() const override;

        // Destroy the backend resource for this control.
        void destroy() const override;

        // Show the already-created backend resource.
        void show() const override;

        // User-originated selection, text, and popup notifications.
        signal<int> on_selection_change;
        signal<std::string> on_text_change;
        signal<bool> on_drop_down;

    protected:
        // Draw the complete themed fallback control.
        virtual void draw_control(gpx &graphics,
                                  theme &appearance,
                                  const rect &bounds,
                                  const theme::state &state);

        // Apply the cached items to an existing native widget.
        virtual void apply_items();

        // Apply the cached selection to an existing native widget.
        virtual void apply_selected_index();

        // Apply the cached text to an existing native widget.
        virtual void apply_text();

        // Apply the cached editing style to an existing native widget.
        virtual void apply_style();

    private:
        friend class detail::control_render_access;

        std::vector<std::string> _items;
        combo_box_style _style;
        int _selected_index = -1;
        std::string _text;

        // Reject a selection outside -1 and the item range.
        void validate_index(int index) const;
    };
} // namespace native
