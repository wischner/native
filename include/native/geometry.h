//
// Declares backend-neutral coordinates, colors, and geometry values.
// Color packing is defined explicitly so behavior does not depend on
// host byte order or compiler extensions. Small constexpr color
// operations stay inline so callers can use them at compile time.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstdint>

namespace native
{
    // Screen coordinate used by the public API.
    using coord = std::int16_t;

    // Non-negative screen dimension used by the public API.
    using dim = std::uint16_t;

    // Stores an eight-bit red, green, blue, and alpha color.
    struct rgba
    {
        std::uint8_t r;
        std::uint8_t g;
        std::uint8_t b;
        std::uint8_t a;

        // Construct a transparent black color.
        constexpr rgba() : r(0), g(0), b(0), a(0) {}

        //
        // Construct a color from its individual channels.
        //
        // Parameters:
        //      red         - Red channel.
        //      green       - Green channel.
        //      blue        - Blue channel.
        //      alpha       - Alpha channel.
        //
        constexpr rgba(std::uint8_t red,
                       std::uint8_t green,
                       std::uint8_t blue,
                       std::uint8_t alpha)
            : r(red), g(green), b(blue), a(alpha) {}

        //
        // Construct a color from 0xAABBGGRR packed channel data.
        //
        // Parameters:
        //      packed      - Color packed independently of host byte
        //                    order.
        //
        constexpr rgba(std::uint32_t packed)
            : r(static_cast<std::uint8_t>(packed & 0xffU)),
              g(static_cast<std::uint8_t>((packed >> 8U) & 0xffU)),
              b(static_cast<std::uint8_t>((packed >> 16U) & 0xffU)),
              a(static_cast<std::uint8_t>((packed >> 24U) & 0xffU)) {}

        //
        // Pack the channels as 0xAABBGGRR.
        //
        // Returns:
        //      Packed color independent of host byte order.
        //
        constexpr operator std::uint32_t() const {
            return static_cast<std::uint32_t>(r) |
                   (static_cast<std::uint32_t>(g) << 8U) |
                   (static_cast<std::uint32_t>(b) << 16U) |
                   (static_cast<std::uint32_t>(a) << 24U);
        }
    };

    // Identifies a point in two-dimensional coordinates.
    struct point
    {
        coord x = 0;
        coord y = 0;

        // Construct a point at the origin.
        point();

        //
        // Construct a point from horizontal and vertical coordinates.
        //
        point(coord x_value, coord y_value);
    };

    // Represents a closed line segment between two points.
    struct line
    {
        point a;
        point b;

        // Construct a zero-length line at the origin.
        line();

        //
        // Construct a line from its endpoints.
        //
        line(point first, point second);

        //
        // Construct a line from the coordinates of both endpoints.
        //
        line(coord x1, coord y1, coord x2, coord y2);

        //
        // Determine whether a point lies on this line segment.
        //
        // Returns:
        //      True when the point is collinear and between the
        //      endpoints.
        //
        bool contains(point candidate) const;
    };

    // Stores a width and height.
    struct size
    {
        dim w = 0;
        dim h = 0;

        // Construct an empty size.
        size();

        // Construct a size from a width and height.
        size(dim width, dim height);
    };

    // Stores a half-open rectangle as an origin and size.
    struct rect
    {
        point p;
        size d;

        // Construct an empty rectangle at the origin.
        rect();

        // Construct a rectangle from an origin and size.
        rect(point origin, size dimensions);

        // Construct a rectangle from scalar coordinates and dimensions.
        rect(coord x, coord y, dim width, dim height);

        // Return the inclusive left coordinate.
        coord x1() const;

        // Return the inclusive top coordinate.
        coord y1() const;

        // Return the exclusive right coordinate.
        coord x2() const;

        // Return the exclusive bottom coordinate.
        coord y2() const;

        // Return the rectangle width.
        dim w() const;

        // Return the rectangle height.
        dim h() const;

        // Determine whether a point is inside this half-open rectangle.
        bool contains(point candidate) const;

        //
        // Calculate the overlap with another rectangle.
        //
        // Returns:
        //      The overlap, or an empty rectangle when none exists.
        //
        rect intersect(const rect &other) const;
    };
}
