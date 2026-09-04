//
// Declares owned RGBA images and the backend-neutral drawing interface.
// Concrete window and image graphics contexts are supplied by backends.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "font.h"
#include "geometry.h"

namespace native
{
    class gpx;

    // Selects pixel sampling for scaled image drawing.
    enum class image_filter
    {
        nearest,
        linear
    };

    // Selects logical horizontal text alignment.
    enum class text_align
    {
        start,
        center,
        end
    };

    // Selects vertical text alignment within a rectangle.
    enum class text_valign
    {
        top,
        center,
        bottom
    };

    // Selects clipping or end-ellipsis overflow behavior.
    enum class text_overflow
    {
        clip,
        ellipsis
    };

    // Describes bounded plain-text placement.
    struct text_layout
    {
        text_align horizontal = text_align::start;
        text_valign vertical = text_valign::top;
        text_overflow overflow = text_overflow::clip;
        bool single_line = true;
    };

    // Restores a borrowed graphics context's complete portable state.
    class gpx_state final
    {
    public:
        // Capture ink, paper, pen, font, and clipping state.
        explicit gpx_state(gpx &graphics);

        // Restore captured state unless ownership was moved.
        ~gpx_state();

        // Move responsibility for restoring captured state.
        gpx_state(gpx_state &&other) noexcept;

        // State guards cannot be copy constructed.
        gpx_state(const gpx_state &) = delete;

        // State guards cannot be copied or reassigned.
        gpx_state &operator=(const gpx_state &) = delete;

        // State guards cannot replace an active captured state.
        gpx_state &operator=(gpx_state &&) = delete;

    private:
        gpx *_graphics;
        rgba _ink;
        rgba _paper;
        std::uint8_t _pen;
        const font_t *_font;
        rect _clip;
    };

    // Selects an encoded image representation.
    enum class image_format
    {
        png,
        jpeg
    };

    // Owns a contiguous RGBA pixel buffer and its graphics context.
    class img
    {
    public:
        //
        // Create an image with non-zero dimensions.
        //
        // Throws:
        //      std::invalid_argument when a dimension is zero or cannot
        //      be represented by a positive Native coordinate.
        //
        img(dim width, dim height);

        // Destroy the image and its lazily created graphics context.
        ~img();

        // Images cannot be copied because they own their pixel storage.
        img(const img &) = delete;

        // Images cannot be copy-assigned.
        img &operator=(const img &) = delete;

        // Decode a PNG or JPEG file, selected from its encoded
        // contents.
        static img load(const std::filesystem::path &path);

        // Decode a PNG or JPEG held in memory.
        static img decode(const std::uint8_t *data, std::size_t size);

        // Encode this image into a PNG or JPEG memory buffer.
        std::vector<std::uint8_t> encode(image_format format,
                                         int jpeg_quality = 90) const;

        // Encode this image to a .png, .jpg, or .jpeg file.
        void save(const std::filesystem::path &path,
                  int jpeg_quality = 90) const;

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
        // Adopt decoded RGBA storage without exposing a resize
        // operation.
        img(dim width, dim height, std::unique_ptr<rgba[]> pixels);

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

        // Capture portable drawing state in a move-only RAII guard.
        [[nodiscard]] gpx_state save_state();

        // Return metrics for the currently selected font.
        font_metrics get_font_metrics() const;

        // Measure UTF-8 text using the currently selected font.
        text_metrics measure_text(const std::string &text) const;

        // Measure one Unicode character using the selected font.
        text_metrics measure_character(char32_t character) const;

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

        // Draw an ellipse outline or fill using portable geometry.
        gpx &draw_ellipse(const rect &bounds, bool filled = false);

        // Draw connected line segments through all supplied points.
        gpx &draw_polyline(const std::vector<point> &points);

        // Draw an outlined or filled polygon.
        gpx &draw_polygon(const std::vector<point> &points,
                          bool filled = false);

        // Draw text from a top-left position using the selected
        // color/font.
        gpx &draw_text(const std::string &text, point position);

        // Draw clipped, aligned plain text inside a rectangle.
        gpx &draw_text(const std::string &text,
                       const rect &bounds,
                       const text_layout &layout = {});

        // Draw an image at a destination point.
        virtual gpx &draw_img(const img &source, point destination) = 0;

        // Scale a complete image into a destination rectangle.
        gpx &draw_img(const img &source,
                      const rect &destination,
                      image_filter filter = image_filter::linear);

        // Crop and scale a source rectangle into a destination.
        gpx &draw_img(const img &source,
                      const rect &source_rect,
                      const rect &destination,
                      image_filter filter = image_filter::linear);

    protected:
        // Draw stock-font text through the selected native backend.
        virtual gpx &draw_native_text(
            const std::string &text,
            point position) = 0;

        rgba _ink = rgba(0, 0, 0, 255);
        rgba _paper = rgba(255, 255, 255, 255);
        std::uint8_t _thickness = 1;

        // Non-owning; null selects the stock system font.
        const font_t *_font = nullptr;
    };
} // namespace native
