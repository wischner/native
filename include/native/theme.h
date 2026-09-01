//
// Declares the backend-owned native-look drawing interface.
// Concrete themes live in platform or toolkit implementation
// directories.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "graphics.h"

namespace native
{
    // Selects a semantic native-looking background or frame.
    enum class surface_kind
    {
        panel,
        content,
        inset,
        popup,
        header
    };

    // Selects row-shaped or tile-shaped native selection painting.
    enum class selection_shape
    {
        row,
        tile
    };

    // Selects the direction represented by a disclosure indicator.
    enum class disclosure_state
    {
        collapsed,
        expanded
    };

    // Selects a separator's logical orientation.
    enum class separator_orientation
    {
        horizontal,
        vertical
    };

    // Selects a scrollbar's logical orientation.
    enum class scrollbar_orientation
    {
        horizontal,
        vertical
    };

    // Selects one independently paintable scrollbar part.
    enum class scrollbar_part
    {
        track,
        thumb,
        decrement,
        increment
    };

    // Defines semantic drawing operations implemented by the active
    // backend.
    class theme
    {
    public:
        // Describes the interaction state of a themed element.
        struct state
        {
            bool hot = false;
            bool pressed = false;
            bool selected = false;
            bool disabled = false;
            bool focused = false;
            bool active = true;
        };

        // Stores dimensions selected by the active platform or toolkit.
        struct metrics
        {
            int menu_bar_height = 20;
            int menu_item_height = 20;
            int popup_width = 180;
            int text_padding_x = 8;
            int check_height = 22;
            int radio_height = 22;
            int list_item_height = 20;
            int focus_inset = 2;
            int disclosure_size = 12;
            int header_height = 24;
            int header_padding_x = 6;
            int header_gap = 4;
            int icon_view_padding_x = 6;
            int icon_view_padding_y = 6;
            int icon_view_item_gap_x = 4;
            int icon_view_item_gap_y = 4;
            int icon_view_label_gap = 4;
            int icon_view_min_item_width = 80;
            int separator_extent = 1;
            int scrollbar_extent = 16;
            int scrollbar_min_thumb = 16;
        };

        // Stores colors used when a backend must emulate native
        // drawing.
        struct palette
        {
            rgba button_bg;
            rgba button_border;
            rgba button_highlight;
            rgba button_shadow;
            rgba button_text;
            rgba button_disabled_text;
            rgba button_hot_bg;
            rgba button_hot_text;
            rgba button_pressed_bg;
            rgba button_pressed_text;

            rgba menu_bar_bg;
            rgba menu_bar_line_top;
            rgba menu_bar_line_bottom;
            rgba menu_text;
            rgba menu_disabled_text;
            rgba menu_hot_bg;
            rgba menu_hot_text;
            rgba menu_popup_bg;
            rgba menu_popup_border;

            rgba content_bg;
            rgba content_text;
            rgba selection_bg;
            rgba selection_text;
            rgba selection_inactive_bg;
            rgba selection_inactive_text;
            rgba separator;
            rgba focus;
        };

        // Destroy the theme interface without owning its graphics
        // context.
        virtual ~theme();

        // Theme instances cannot be copied because they borrow one
        // context.
        theme(const theme &) = delete;

        // Theme instances cannot be copy-assigned.
        theme &operator=(const theme &) = delete;

        // Construct the active backend's theme around a borrowed
        // context.
        static std::unique_ptr<theme> create(gpx &painter);

        // Return dimensions selected by the active backend.
        virtual metrics defaults() const = 0;

        // Return colors selected by the active backend.
        virtual palette native_palette() const = 0;

        // Draw a button in its default state.
        theme &draw_button(const rect &bounds, const std::string &text);

        // Draw a complete button in the specified state.
        virtual theme &draw_button(const rect &bounds,
                                   const std::string &text,
                                   const state &element_state) = 0;

        // Draw a complete empty menu bar.
        virtual theme &draw_menu_bar(const rect &bounds) = 0;

        // Draw a menu title in its default state.
        theme &draw_menu_title(const rect &bounds,
                               const std::string &text);

        // Draw a menu title in the specified state.
        virtual theme &draw_menu_title(const rect &bounds,
                                       const std::string &text,
                                       const state &element_state) = 0;

        // Draw a popup-menu item in its default state.
        theme &draw_menu_item(const rect &bounds,
                              const std::string &text);

        // Draw a popup-menu item in the specified state.
        virtual theme &draw_menu_item(const rect &bounds,
                                      const std::string &text,
                                      const state &element_state) = 0;

        // Draw a popup-menu background and frame.
        virtual theme &draw_popup_frame(const rect &bounds) = 0;

        // Draw a list item in its default state.
        theme &draw_list_item(const rect &bounds,
                              const std::string &text);

        // Draw a list item in the specified state.
        virtual theme &draw_list_item(const rect &bounds,
                                      const std::string &text,
                                      const state &element_state) = 0;

        // Draw a check control in its default state.
        theme &draw_check(const rect &bounds, const std::string &text);

        // Draw a complete check control; state.selected is the checked
        // state.
        virtual theme &draw_check(const rect &bounds,
                                  const std::string &text,
                                  const state &element_state) = 0;

        // Draw a radio control in its default state.
        theme &draw_radio(const rect &bounds, const std::string &text);

        // Draw a complete radio control; state.selected marks the
        // chosen item.
        virtual theme &draw_radio(const rect &bounds,
                                  const std::string &text,
                                  const state &element_state) = 0;

        // Draw a single-selection list control in its default state.
        theme &draw_list(const rect &bounds,
                         const std::vector<std::string> &items,
                         int selected_index = -1);

        // Draw a complete list control and its selected item.
        virtual theme &draw_list(const rect &bounds,
                                 const std::vector<std::string> &items,
                                 int selected_index,
                                 const state &element_state) = 0;

        // Draw an empty editable-text frame in its default state.
        theme &draw_text_edit_frame(const rect &bounds);

        // Draw an empty editable-text frame; state.selected is focus.
        virtual theme &draw_text_edit_frame(
            const rect &bounds,
            const state &element_state) = 0;

        // Draw a semantic native-looking surface or frame.
        virtual theme &draw_surface(
            const rect &bounds,
            surface_kind kind,
            const state &element_state);

        // Draw a row or tile selection background.
        virtual theme &draw_selection(
            const rect &bounds,
            selection_shape shape,
            const state &element_state);

        // Draw a keyboard-focus indicator.
        virtual theme &draw_focus(
            const rect &bounds,
            const state &element_state);

        // Draw a collapsed or expanded disclosure indicator.
        virtual theme &draw_disclosure(
            const rect &bounds,
            disclosure_state disclosure,
            const state &element_state);

        // Draw a native separator across its supplied bounds.
        virtual theme &draw_separator(
            const rect &bounds,
            separator_orientation orientation);

        // Draw one semantic native-looking scrollbar part.
        virtual theme &draw_scrollbar_part(
            const rect &bounds,
            scrollbar_orientation orientation,
            scrollbar_part part,
            const state &element_state);

    protected:
        // Construct a theme that borrows its destination context.
        explicit theme(gpx &painter);

        // Draw a semantic surface through portable graphics.
        theme &draw_surface_fallback(
            const rect &bounds,
            surface_kind kind,
            const state &element_state);

        // Draw a semantic selection through portable graphics.
        theme &draw_selection_fallback(
            const rect &bounds,
            selection_shape shape,
            const state &element_state);

        // Draw a focus indicator through portable graphics.
        theme &draw_focus_fallback(
            const rect &bounds,
            const state &element_state);

        // Draw a disclosure indicator through portable graphics.
        theme &draw_disclosure_fallback(
            const rect &bounds,
            disclosure_state disclosure,
            const state &element_state);

        // Draw a separator through portable graphics.
        theme &draw_separator_fallback(
            const rect &bounds,
            separator_orientation orientation);

        // Draw a scrollbar part through portable graphics.
        theme &draw_scrollbar_part_fallback(
            const rect &bounds,
            scrollbar_orientation orientation,
            scrollbar_part part,
            const state &element_state);

        // Borrowed for the lifetime of this short-lived theme instance.
        gpx &_g;
    };
} // namespace native
