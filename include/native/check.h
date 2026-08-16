//
// Declares the portable two-state check control.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>

#include "wnd.h"

namespace native
{
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
        void on_native_checked(bool checked);

        // Create the backend check resource.
        void create() const override;

        // Destroy the backend check resource.
        void destroy() const override;

        // Show the backend check resource.
        void show() const override;

        // Emits the checked state after a user-originated change.
        signal<bool> on_change;

    private:
        std::string _text;
        bool _checked = false;

        void apply_text();
        void apply_checked();
    };
} // namespace native
