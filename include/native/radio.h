//
// Declares the portable mutually-exclusive radio control.
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

    class radio : public wnd
    {
    public:
        // Construct a radio control from a label and scalar bounds.
        radio(std::string text,
              coord x = 0,
              coord y = 0,
              dim width = 120,
              dim height = 24);

        // Construct a radio control from a label, position, and size.
        radio(const std::string &text,
              const point &position,
              const size &dimensions);

        // Construct a radio control from a label and complete bounds.
        radio(const std::string &text, const rect &bounds);

        // Destroy the control and its native resource if it exists.
        ~radio() override;

        // Return the cached control label.
        const std::string &get_text() const;

        // Change the label and update a created native control.
        radio &set_text(const std::string &text);

        // Return whether this control is selected.
        bool get_selected() const;

        // Select or clear the control without emitting an action
        // signal.
        radio &set_selected(bool selected);

        // Select this control after a native user action and notify
        // changes.
        virtual void on_native_selected();

        // Create the backend radio resource.
        void create() const override;

        // Destroy the backend radio resource.
        void destroy() const override;

        // Show the backend radio resource.
        void show() const override;

        // Emits selection changes caused by a user action in the group.
        signal<bool> on_change;

    protected:
        // Draw the complete radio using the active native theme.
        virtual void draw_control(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        // Apply the cached label to a created native control.
        virtual void apply_text();

        // Apply the cached selected state to a native control.
        virtual void apply_selected();

        // Select this radio and update its sibling group.
        virtual void select_exclusive(bool notify);

        // Notify one user-originated group state transition.
        virtual void selection_changed(bool selected);

    private:
        friend class detail::control_render_access;

        std::string _text;
        bool _selected = false;
    };
} // namespace native
