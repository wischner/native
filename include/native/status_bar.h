//
// Declares a themed bottom-edge window status bar.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>
#include <vector>

#include "non_client.h"
#include "theme.h"

namespace native
{
    struct status_bar_part
    {
        std::string text;
        int width = 0;
    };

    class status_bar : public non_client
    {
    public:
        explicit status_bar(wnd &owner, int height = 22);

        const std::string &get_text() const;
        status_bar &set_text(const std::string &text);

        const std::vector<status_bar_part> &get_parts() const;
        status_bar &set_parts(std::vector<status_bar_part> parts);

    protected:
        void draw(gpx &graphics, const rect &bounds) override;

        virtual void draw_background(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        virtual void draw_part(gpx &graphics,
                               theme &appearance,
                               const rect &bounds,
                               const status_bar_part &part,
                               const theme::state &state);

    private:
        std::string _text;
        std::vector<status_bar_part> _parts;
    };
} // namespace native
