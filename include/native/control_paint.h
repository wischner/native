//
// Declares a backend-aware painter for buttons, menus, and list items.
// Native toolkit rendering is preferred and portable drawing is used as
// a fallback so controls retain a consistent semantic state model.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>

#include "graphics.h"

namespace native
{
    // Draws common control parts with native or fallback styling.
    class control_paint
    {
    public:
        // Describes the interaction state of a control part.
        struct state
        {
            bool hot = false;
            bool pressed = false;
            bool selected = false;
            bool disabled = false;
        };

        // Stores backend-selected control dimensions.
        struct metrics
        {
            int menu_bar_height = 20;
            int menu_item_height = 20;
            int popup_width = 180;
            int text_padding_x = 8;
        };

        // Stores colors used by the portable control renderer.
        struct palette
        {
            rgba button_bg;
            rgba button_border;
            rgba button_text;
            rgba button_hot_bg;
            rgba button_hot_text;
            rgba button_pressed_bg;
            rgba button_pressed_text;

            rgba menu_bar_bg;
            rgba menu_bar_line_top;
            rgba menu_bar_line_bottom;
            rgba menu_text;
            rgba menu_hot_bg;
            rgba menu_hot_text;
            rgba menu_popup_bg;
            rgba menu_popup_border;
        };

        // Construct a control painter around a borrowed context.
        explicit control_paint(gpx &painter);

        // Return the current backend's control dimensions.
        metrics defaults() const;

        // Return the current backend's control colors.
        static palette native_palette();

        // Draw the background portion of a button.
        control_paint &draw_button_face(
            const rect &bounds,
            const state &control_state);

        // Draw the border portion of a button.
        control_paint &draw_button_frame(
            const rect &bounds,
            const state &control_state);

        // Draw a button label centered within its bounds.
        control_paint &draw_button_text(
            const rect &bounds,
            const std::string &text,
            const state &control_state);

        // Draw a button in its default state.
        control_paint &draw_button(
            const rect &bounds,
            const std::string &text);

        // Draw a complete button in a specified state.
        control_paint &draw_button(
            const rect &bounds,
            const std::string &text,
            const state &control_state);

        // Draw a menu bar background.
        control_paint &draw_menu_bar_background(const rect &bounds);

        // Draw a menu item's state-sensitive background.
        control_paint &draw_menu_item_background(
            const rect &bounds,
            const state &control_state);

        // Draw a menu item's state-sensitive label.
        control_paint &draw_menu_item_text(
            const rect &bounds,
            const std::string &text,
            const state &control_state);

        // Draw a complete empty menu bar.
        control_paint &draw_menu_bar(const rect &bounds);

        // Draw a menu title in its default state.
        control_paint &draw_menu_title(
            const rect &bounds,
            const std::string &text);

        // Draw a menu title in a specified state.
        control_paint &draw_menu_title(
            const rect &bounds,
            const std::string &text,
            const state &control_state);

        // Draw a popup menu item in its default state.
        control_paint &draw_menu_item(
            const rect &bounds,
            const std::string &text);

        // Draw a popup menu item in a specified state.
        control_paint &draw_menu_item(
            const rect &bounds,
            const std::string &text,
            const state &control_state);

        // Draw a popup menu background and border.
        control_paint &draw_popup_frame(const rect &bounds);

        // Draw a list item with the default menu-item appearance.
        control_paint &draw_list_item(
            const rect &bounds,
            const std::string &text);

        // Draw a list item with state-sensitive menu-item appearance.
        control_paint &draw_list_item(
            const rect &bounds,
            const std::string &text,
            const state &control_state);

    private:
        gpx &_g;
    };

    // Expose the architecture's concise name for control drawing.
    using theme = control_paint;
}
