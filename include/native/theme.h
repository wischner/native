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
    enum class ruler_orientation;

    // Selects a semantic native-looking background or frame.
    enum class surface_kind
    {
        panel,
        content,
        inset,
        popup,
        header,
        table_header,
        // Native status-strip background and individual cell divider.
        status,
        status_part
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

    // Selects the direction represented by a table sort indicator.
    enum class sort_indicator_state
    {
        ascending,
        descending
    };

    // Selects one compact tool-window caption button.
    enum class caption_button_kind
    {
        close,
        pin,
        unpin
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
            int button_height = 28;
            int menu_bar_height = 20;
            int menu_item_height = 20;
            int popup_width = 180;
            int text_padding_x = 8;
            int text_edit_height = 24;
            int check_height = 22;
            int radio_height = 22;
            int list_item_height = 20;
            // Default full table-row height, including native vertical
            // breathing room. An explicit table_view row height overrides
            // this value.
            int table_row_height = 20;
            // Width of a final inset table-viewport relief drawn after all
            // table parts. Zero leaves the frame to a native peer or theme.
            int table_outer_border_extent = 0;
            int focus_inset = 2;
            int disclosure_size = 12;
            int sort_indicator_size = 8;
            int caption_button_size = 16;
            // Whether a tree draws classic hierarchy connector branches by
            // default. Controls retain an explicit set_lines_visible()
            // override across later native metric synchronization.
            bool tree_lines_visible = true;
            // Tree-specific geometry. Negative/zero sentinel values retain
            // the generic list/header-derived geometry.
            int tree_row_height = 0;
            int tree_horizontal_padding = -1;
            int tree_indent_width = 0;
            int tree_item_gap = -1;
            int tree_icon_vertical_padding = 4;
            int header_height = 24;
            int header_padding_x = 6;
            int header_gap = 4;
            int tab_height = 24;
            int icon_view_padding_x = 6;
            int icon_view_padding_y = 6;
            int icon_view_item_gap_x = 4;
            int icon_view_item_gap_y = 4;
            int icon_view_label_gap = 4;
            int icon_view_min_item_width = 80;
            int separator_extent = 1;
            int scrollbar_extent = 16;
            int scrollbar_min_thumb = 16;
            int status_bar_height = 22;
            int ruler_extent = 24;
            // Native table implementations that conventionally consume
            // unused header space can stretch the trailing visible column
            // without changing its semantic/model width.
            bool table_fill_last_column = false;
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
            // Optional native alternating-row background. A transparent
            // value asks the shared table painter to derive one.
            rgba content_alt_bg;
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

        // Return individual native dimensions used by themed shapes.
        int get_button_height() const;
        int get_menu_bar_height() const;
        int get_menu_item_height() const;
        int get_popup_width() const;
        int get_text_padding_x() const;
        int get_text_edit_height() const;
        int get_check_height() const;
        int get_radio_height() const;
        int get_list_item_height() const;
        int get_table_row_height() const;
        int get_table_outer_border_extent() const;
        int get_focus_inset() const;
        int get_disclosure_size() const;
        int get_sort_indicator_size() const;
        int get_caption_button_size() const;
        bool get_tree_lines_visible() const;
        int get_tree_row_height() const;
        int get_tree_horizontal_padding() const;
        int get_tree_indent_width() const;
        int get_tree_item_gap() const;
        int get_tree_icon_vertical_padding() const;
        int get_header_height() const;
        int get_header_padding_x() const;
        int get_header_gap() const;
        int get_tab_height() const;
        int get_icon_view_padding_x() const;
        int get_icon_view_padding_y() const;
        int get_icon_view_item_gap_x() const;
        int get_icon_view_item_gap_y() const;
        int get_icon_view_label_gap() const;
        int get_icon_view_min_item_width() const;
        int get_separator_extent() const;
        int get_scrollbar_extent() const;
        int get_scrollbar_min_thumb() const;
        int get_status_bar_height() const;
        int get_ruler_extent() const;
        bool get_table_fill_last_column() const;

        // Return complete bounds for variable-length linear shapes. The
        // caller supplies the length along the shape's main axis.
        size get_separator_size(separator_orientation orientation,
                                int length) const;
        size get_scrollbar_size(scrollbar_orientation orientation,
                                int length) const;
        size get_status_bar_size(int width) const;
        size get_ruler_size(ruler_orientation orientation,
                            int length) const;

        // Return colors selected by the active backend.
        virtual palette native_palette() const = 0;

        // Return opaque colors for custom compositions. Unlike the raw
        // palette, these getters never use transparency as a sentinel.
        rgba get_button_background_color() const;
        rgba get_button_border_color() const;
        rgba get_button_highlight_color() const;
        rgba get_button_shadow_color() const;
        rgba get_button_foreground_color() const;
        rgba get_button_disabled_foreground_color() const;
        rgba get_button_hot_background_color() const;
        rgba get_button_hot_foreground_color() const;
        rgba get_button_pressed_background_color() const;
        rgba get_button_pressed_foreground_color() const;
        rgba get_menu_bar_background_color() const;
        rgba get_menu_bar_top_color() const;
        rgba get_menu_bar_bottom_color() const;
        rgba get_menu_foreground_color() const;
        rgba get_menu_disabled_foreground_color() const;
        rgba get_menu_hot_background_color() const;
        rgba get_menu_hot_foreground_color() const;
        rgba get_menu_popup_background_color() const;
        rgba get_menu_popup_border_color() const;
        rgba get_content_background_color() const;
        rgba get_content_alternate_background_color() const;
        rgba get_content_foreground_color() const;
        rgba get_selection_background_color() const;
        rgba get_selection_foreground_color() const;
        rgba get_inactive_selection_background_color() const;
        rgba get_inactive_selection_foreground_color() const;
        rgba get_separator_color() const;
        rgba get_focus_color() const;

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

        // Draw an ascending or descending native sort indicator.
        virtual theme &draw_sort_indicator(
            const rect &bounds,
            sort_indicator_state direction,
            const state &element_state);

        // Draw one native compact-caption command button.
        virtual theme &draw_caption_button(
            const rect &bounds,
            caption_button_kind kind,
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

        // Draw a sort indicator through portable graphics.
        theme &draw_sort_indicator_fallback(
            const rect &bounds,
            sort_indicator_state direction,
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
