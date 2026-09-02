//
// Implements backend-neutral rotated text through a transparent image.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "rotated_text.h"

#include <algorithm>
#include <limits>

namespace
{
    native::coord to_coord(int value) {
        const int low =
            static_cast<int>(std::numeric_limits<native::coord>::min());
        const int high =
            static_cast<int>(std::numeric_limits<native::coord>::max());
        return static_cast<native::coord>(
            std::max(low, std::min(high, value)));
    }

    native::dim to_dim(int value) {
        if (value <= 0)
            return 0;
        const int high =
            static_cast<int>(std::numeric_limits<native::dim>::max());
        return static_cast<native::dim>(std::min(high, value));
    }
}

namespace native::detail
{
    void draw_rotated_text(gpx &graphics,
                           const std::string &text,
                           const rect &bounds,
                           bool clockwise,
                           int padding) {
        const int source_width = std::max(
            1, static_cast<int>(bounds.d.h) - padding * 2);
        const int source_height = std::max(
            1, static_cast<int>(bounds.d.w) - 4);
        img source(to_dim(source_width), to_dim(source_height));
        gpx &source_graphics = source.get_gpx();
        const rgba ink = graphics.get_ink();

        // Alpha-zero complementary paper stays transparent while letting
        // native image-font adapters detect black and white glyph pixels.
        const rgba transparent_paper(
            static_cast<std::uint8_t>(255U - ink.r),
            static_cast<std::uint8_t>(255U - ink.g),
            static_cast<std::uint8_t>(255U - ink.b),
            0);
        source_graphics
            .clear(transparent_paper)
            .set_font(graphics.get_font())
            .set_ink(ink)
            .draw_text(
                text,
                rect(0, 0, to_dim(source_width),
                     to_dim(source_height)),
                text_layout{text_align::center,
                            text_valign::center,
                            text_overflow::ellipsis,
                            true});

        img rotated(to_dim(source_height), to_dim(source_width));
        rgba *destination = rotated.pixels();
        const rgba *origin = source.pixels();
        for (int y = 0; y < source_height; ++y) {
            for (int x = 0; x < source_width; ++x) {
                const int target_x = clockwise
                    ? source_height - 1 - y : y;
                const int target_y = clockwise
                    ? x : source_width - 1 - x;
                destination[target_y * source_height + target_x] =
                    origin[y * source_width + x];
            }
        }
        graphics.draw_img(
            rotated,
            point(
                to_coord(bounds.p.x +
                    (static_cast<int>(bounds.d.w) -
                     static_cast<int>(rotated.w())) / 2),
                to_coord(bounds.p.y +
                    (static_cast<int>(bounds.d.h) -
                     static_cast<int>(rotated.h())) / 2)));
    }
} // namespace native::detail
