//
// Declares target-independent RGBA image drawing primitives used by
// memory-backed graphics contexts.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <native/graphics.h>

namespace native::detail
{
    // Return the complete bounds of an image target.
    rect image_bounds(const img &target);

    // Replace one pixel when it lies inside the target and active clip.
    void put_image_pixel(
        const img &target, const rect &clip, int x, int y, rgba color);

    // Fill the clipped part of an image with one color.
    void clear_image(const img &target, const rect &clip, rgba color);

    // Draw a clipped line into an image.
    void draw_image_line(const img &target,
                         const rect &clip,
                         point from,
                         point to,
                         rgba color,
                         std::uint8_t thickness);

    // Draw a clipped outlined or filled rectangle into an image.
    void draw_image_rect(const img &target,
                         const rect &clip,
                         const rect &bounds,
                         rgba color,
                         std::uint8_t thickness,
                         bool filled);

    // Alpha-compose one image over another at a destination point.
    void copy_image(const img &target,
                    const rect &clip,
                    const img &source,
                    point destination);
} // namespace native::detail
