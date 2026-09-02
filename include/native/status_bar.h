//
// Declares a themed bottom-edge window status bar.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <memory>
#include <string>
#include <vector>

#include "non_client.h"
#include "theme.h"

namespace native
{
    namespace detail
    {
        class status_bar_peer;
    }

    struct status_bar_part
    {
        std::string text;
        int width = 0;
    };

    class status_bar : public non_client
    {
    public:
        // Construct a bottom-edge status bar with a fixed height.
        explicit status_bar(wnd &owner, int height = 22);
        ~status_bar() override;

        // Return the single flexible status text.
        const std::string &get_text() const;

        // Replace the bar with one flexible text part.
        status_bar &set_text(const std::string &text);

        // Return the ordered fixed and flexible status parts.
        const std::vector<status_bar_part> &get_parts() const;

        // Replace the bar with ordered status parts.
        status_bar &set_parts(std::vector<status_bar_part> parts);

    protected:
        // Paint the complete bar and its resolved parts.
        void draw(gpx &graphics, const rect &bounds) override;

        void on_configuration_changed() override;

        // Draw the native-themed bar background.
        virtual void draw_background(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        // Draw one resolved fixed or flexible status part.
        virtual void draw_part(gpx &graphics,
                               theme &appearance,
                               const rect &bounds,
                               const status_bar_part &part,
                               const theme::state &state);

    private:
        std::string _text;
        std::vector<status_bar_part> _parts;
        std::unique_ptr<detail::status_bar_peer> _native_peer;

        bool synchronize_native(const rect &bounds);
    };
} // namespace native
