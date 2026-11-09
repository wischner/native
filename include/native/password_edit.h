//
// Declares a single-line secret entry that never paints its value.
// Toolkit fields differ in whether they can mask at all, so one shared
// painted control behaves the same on every backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>

#include "canvas.h"
#include "theme.h"

namespace native
{
    // Accepts a secret, showing one bullet for each character typed.
    class password_edit : public canvas
    {
    public:
        // Construct an empty field from scalar bounds.
        password_edit(coord x = 0,
                      coord y = 0,
                      dim width = 180,
                      dim height = 26);

        // Clear the secret from memory.
        ~password_edit() override;

        // Return the current UTF-8 secret.
        const std::string &get_text() const;

        // Replace the secret without emitting on_change.
        password_edit &set_text(const std::string &text);

        // Return how many characters the field shows.
        std::size_t get_length() const;

        // Accept composed text; control characters are refused.
        bool on_native_text_input(const std::string &utf8) override;

        // Delete the last character, or paste on Ctrl+V.
        bool on_native_key(key_event event) override;

        // Repaint when the field gains or loses keyboard focus.
        void on_native_focus(bool focused) override;

        // Emits the secret after a user-originated change.
        signal<std::string> on_change;

    protected:
        // Draw the field frame, one bullet per character and the caret.
        virtual void draw_control(gpx &graphics,
                                  theme &appearance,
                                  const rect &bounds,
                                  const theme::state &state);

    private:
        // Paint the current state.
        void paint(wnd_paint_event event);

        // Append accepted text and report the change.
        bool append(const std::string &utf8);

        std::string _text;
        bool _focused = false;
    };
}
