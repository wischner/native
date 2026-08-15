//
// Declares the internal graphics context for owned images.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <native.h>

namespace native
{

    class gpx_img : public gpx
    {
    public:
        // Construct a graphics context borrowing a valid image.
        explicit gpx_img(const img &image);

        // Destroy the image graphics interface.
        ~gpx_img() override;

        // Set the image clipping rectangle.
        gpx &set_clip(const rect &r) override;

        // Return the image clipping rectangle.
        rect get_clip() const override;

        // Fill the clipping rectangle with a color.
        gpx &clear(rgba color) override;

        // Draw a line into the image.
        gpx &draw_line(point from, point to) override;

        // Draw an outlined or filled rectangle into the image.
        gpx &draw_rect(rect r, bool filled = false) override;

        // Draw text into the image.
        gpx &draw_text(const std::string &text, point p) override;

        // Copy another image into this image.
        gpx &draw_img(const img &src, point dst) override;

    private:
        // Non-owning reference to the image being painted.
        const img &_img;
        rect _clip;
    };

} // namespace native
