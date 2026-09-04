//
// Declares the portable push-button control. Backends implement the
// native resource lifecycle in their same-named source module.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>

#include "theme.h"
#include "wnd.h"

namespace native
{
    namespace detail
    {
        class control_render_access;
    }

    // Represents a clickable native or emulated push button.
    class button : public wnd
    {
    public:
        // Construct a button from a label and scalar bounds.
        button(std::string text,
               coord x = 0,
               coord y = 0,
               dim width = 96,
               dim height = 28);

        // Construct a button from a label, position, and size.
        button(const std::string &text,
               const point &position,
               const size &dimensions);

        // Construct a button from a label and complete bounds.
        button(const std::string &text, const rect &bounds);

        // Destroy the button and its native resource if it exists.
        ~button() override;

        // Return the cached button label.
        const std::string &get_text() const;

        // Change the label and update a created native button.
        button &set_text(const std::string &text);

        // Accept a native user activation and emit on_click.
        virtual void on_native_click();

    protected:
        // Create the backend button resource.
        void create_native() override;

        // Destroy the backend button resource.
        void destroy_native() override;

        // Show the backend button resource.
        void show_native() override;

    public:

        // Emits when the user activates the button.
        signal<> on_click;

    protected:
        // Dispatch the complete staged button drawing contract.
        virtual void draw_control(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        // Draw the button face without its border or content.
        virtual void draw_background(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        // Draw the button edge after its background.
        virtual void draw_border(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        // Draw the centered button label.
        virtual void draw_text(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        // Draw the keyboard-focus indicator last.
        virtual void draw_focus(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        // Apply the cached label to a created backend button.
        virtual void apply_text();

    private:
        friend class detail::control_render_access;

        std::string _text;
    };
} // namespace native
