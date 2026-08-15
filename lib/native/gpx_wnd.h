//
// Declares the internal graphics context for native windows.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <native.h>

namespace native
{

    class gpx_wnd : public gpx
    {
    public:
        // Construct a context borrowing a window and drawing offset.
        gpx_wnd(const wnd *wnd, point offset = {0, 0});

        // Release backend drawing caches owned by this context.
        ~gpx_wnd() override;

        // Return the borrowed window associated with this context.
        wnd *window() const;

        // Set the window clipping rectangle.
        gpx &set_clip(const rect &r) override;

        // Return the window clipping rectangle.
        rect get_clip() const override;

        // Fill the clipping rectangle with a color.
        gpx &clear(rgba color) override;

        // Draw a line into the window.
        gpx &draw_line(point from, point to) override;

        // Draw an outlined or filled rectangle into the window.
        gpx &draw_rect(rect r, bool filled = false) override;

        // Draw text into the window.
        gpx &draw_text(const std::string &text, point p) override;

        // Draw an image into the window.
        gpx &draw_img(const img &src, point dst) override;

    private:
        wnd *_wnd;
        rect _clip;
        point _offset;
    };

}
