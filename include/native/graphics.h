//
// Declares owned RGBA images and the backend-neutral drawing interface.
// Concrete window and image graphics contexts are supplied by backends.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "font.h"
#include "geometry.h"

namespace native
{
    class gpx;

    // Owns a contiguous RGBA pixel buffer and its graphics context.
    class img
    {
    public:
        //
        // Create an image with non-zero dimensions.
        //
        // Throws:
        //      std::invalid_argument when either dimension is zero.
        //
        img(dim width, dim height);

        // Destroy the image and its lazily created graphics context.
        ~img();

        // Return the image width in pixels.
        coord w() const;

        // Return the image height in pixels.
        coord h() const;

        // Return mutable access to the first pixel.
        rgba *pixels();

        // Return read-only access to the first pixel.
        const rgba *pixels() const;

        // Return the lazily created drawing context for this image.
        gpx &get_gpx() const;

    private:
        coord _w;
        coord _h;
        std::unique_ptr<rgba[]> _data;
        mutable std::unique_ptr<gpx> _gpx;
    };

    // Defines stateful drawing operations shared by all backends.
    class gpx
    {
    public:
        // Destroy a graphics context through its interface.
        virtual ~gpx();

        // Set the foreground drawing color.
        gpx &set_ink(rgba color);

        // Return the foreground drawing color.
        rgba get_ink() const;

        // Set the background drawing color.
        gpx &set_paper(rgba color);

        // Return the background drawing color.
        rgba get_paper() const;

        // Set the line thickness in pixels.
        gpx &set_pen(std::uint8_t thickness);

        // Return the line thickness in pixels.
        std::uint8_t get_pen() const;

        // Select a non-owning font for subsequent text operations.
        gpx &set_font(const font_t &font);

        // Return the selected font or the stock system font.
        const font_t &get_font() const;

        // Set the clipping rectangle for subsequent drawing.
        virtual gpx &set_clip(const rect &bounds) = 0;

        // Return the current clipping rectangle.
        virtual rect get_clip() const = 0;

        // Fill the current clipping rectangle with a color.
        virtual gpx &clear(rgba color) = 0;

        // Draw a line between two points.
        virtual gpx &draw_line(point from, point to) = 0;

        // Draw a rectangle outline or filled rectangle.
        virtual gpx &draw_rect(rect bounds, bool filled = false) = 0;

        // Draw text using the selected color and font.
        virtual gpx &draw_text(
            const std::string &text,
            point position) = 0;

        // Draw an image at a destination point.
        virtual gpx &draw_img(
            const img &source,
            point destination) = 0;

    protected:
        rgba _ink = rgba(0, 0, 0, 255);
        rgba _paper = rgba(255, 255, 255, 255);
        std::uint8_t _thickness = 1;

        // Non-owning; null selects the stock system font.
        const font_t *_font = nullptr;
    };
}
