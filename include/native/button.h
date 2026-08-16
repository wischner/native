//
// Declares the portable push-button control. Backends implement the
// native resource lifecycle in their same-named source module.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>

#include "wnd.h"

namespace native
{
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

        // Create the backend button resource.
        void create() const override;

        // Destroy the backend button resource.
        void destroy() const override;

        // Show the backend button resource.
        void show() const override;

        // Emits when the user activates the button.
        signal<> on_click;

    private:
        std::string _text;

        // Apply the cached label to a created backend button.
        void apply_text();
    };
} // namespace native
