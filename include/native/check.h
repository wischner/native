//
// Declares the portable two-state check control.
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

    class check : public wnd
    {
    public:
        // Construct a check control from a label and scalar bounds.
        check(std::string text,
              coord x = 0,
              coord y = 0,
              dim width = 120,
              dim height = 24);

        // Construct a check control from a label, position, and size.
        check(const std::string &text,
              const point &position,
              const size &dimensions);

        // Construct a check control from a label and complete bounds.
        check(const std::string &text, const rect &bounds);

        // Destroy the control and its native resource if it exists.
        ~check() override;

        // Return the cached control label.
        const std::string &get_text() const;

        // Change the label and update a created native control.
        check &set_text(const std::string &text);

        // Return whether the control is checked.
        bool get_checked() const;

        // Set the checked state without emitting a user-action signal.
        check &set_checked(bool checked);

        // Cache a native-originated state change and emit on_change.
        virtual void on_native_checked(bool checked);

    protected:
        // Create the backend check resource.
        void create_native() override;

        // Destroy the backend check resource.
        void destroy_native() override;

        // Show the backend check resource.
        void show_native() override;

    public:

        // Emits the checked state after a user-originated change.
        signal<bool> on_change;

    protected:
        // Dispatch the complete staged check-control contract.
        virtual void draw_control(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        // Draw the control background.
        virtual void draw_background(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        // Draw the check indicator and its selected state.
        virtual void draw_indicator(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        // Draw the check label.
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

        // Apply the cached label to a created native control.
        virtual void apply_text();

        // Apply the cached checked state to a native control.
        virtual void apply_checked();

    private:
        friend class detail::control_render_access;

        std::string _text;
        bool _checked = false;
    };
} // namespace native
