//
// Implements target-independent RGBA image drawing primitives used by
// every memory-backed graphics context.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "software_image.h"

#include <algorithm>
#include <cstdlib>

namespace
{
    native::rgba blend(native::rgba destination, native::rgba source) {
        const unsigned alpha = source.a;
        const unsigned inverse = 255U - alpha;
        const unsigned output_alpha =
            alpha + (destination.a * inverse + 127U) / 255U;
        if (output_alpha == 0)
            return {};

        const auto channel = [=](unsigned destination_channel,
                                 unsigned source_channel) {
            const unsigned premultiplied =
                source_channel * alpha +
                (destination_channel * destination.a * inverse + 127U) /
                    255U;
            return static_cast<std::uint8_t>(
                (premultiplied + output_alpha / 2U) / output_alpha);
        };
        return native::rgba(channel(destination.r, source.r),
                            channel(destination.g, source.g),
                            channel(destination.b, source.b),
                            static_cast<std::uint8_t>(output_alpha));
    }
} // namespace

namespace native::detail
{
    rect image_bounds(const img &target) {
        return rect(0, 0, target.w(), target.h());
    }

    void put_image_pixel(
        const img &target, const rect &clip, int x, int y, rgba color) {
        if (x < 0 || y < 0 || x >= target.w() || y >= target.h() ||
            x < clip.x1() || y < clip.y1() || x >= clip.x2() ||
            y >= clip.y2()) {
            return;
        }
        const_cast<rgba *>(target.pixels())[y * target.w() + x] = color;
    }

    void clear_image(const img &target, const rect &clip, rgba color) {
        const rect area = clip.intersect(image_bounds(target));
        rgba *pixels = const_cast<rgba *>(target.pixels());
        for (int y = area.y1(); y < area.y2(); ++y) {
            for (int x = area.x1(); x < area.x2(); ++x)
                pixels[y * target.w() + x] = color;
        }
    }

    void draw_image_line(const img &target,
                         const rect &clip,
                         point from,
                         point to,
                         rgba color,
                         std::uint8_t thickness) {
        int x0 = from.x;
        int y0 = from.y;
        const int x1 = to.x;
        const int y1 = to.y;
        const int dx = std::abs(x1 - x0);
        const int sx = x0 < x1 ? 1 : -1;
        const int dy = -std::abs(y1 - y0);
        const int sy = y0 < y1 ? 1 : -1;
        int error = dx + dy;
        const int first = -static_cast<int>((thickness - 1) / 2);
        const int last = static_cast<int>(thickness / 2);

        while (true) {
            for (int offset_y = first; offset_y <= last; ++offset_y) {
                for (int offset_x = first; offset_x <= last;
                     ++offset_x) {
                    put_image_pixel(target,
                                    clip,
                                    x0 + offset_x,
                                    y0 + offset_y,
                                    color);
                }
            }
            if (x0 == x1 && y0 == y1)
                break;
            const int doubled = error * 2;
            if (doubled >= dy) {
                error += dy;
                x0 += sx;
            }
            if (doubled <= dx) {
                error += dx;
                y0 += sy;
            }
        }
    }

    void draw_image_rect(const img &target,
                         const rect &clip,
                         const rect &bounds,
                         rgba color,
                         std::uint8_t thickness,
                         bool filled) {
        if (bounds.w() == 0 || bounds.h() == 0)
            return;
        if (filled) {
            clear_image(target, clip.intersect(bounds), color);
            return;
        }

        const coord right = static_cast<coord>(bounds.x2() - 1);
        const coord bottom = static_cast<coord>(bounds.y2() - 1);
        draw_image_line(target,
                        clip,
                        bounds.p,
                        point(right, bounds.p.y),
                        color,
                        thickness);
        draw_image_line(target,
                        clip,
                        bounds.p,
                        point(bounds.p.x, bottom),
                        color,
                        thickness);
        draw_image_line(target,
                        clip,
                        point(right, bounds.p.y),
                        point(right, bottom),
                        color,
                        thickness);
        draw_image_line(target,
                        clip,
                        point(bounds.p.x, bottom),
                        point(right, bottom),
                        color,
                        thickness);
    }

    void copy_image(const img &target,
                    const rect &clip,
                    const img &source,
                    point destination) {
        rgba *target_pixels = const_cast<rgba *>(target.pixels());
        for (int y = 0; y < source.h(); ++y) {
            for (int x = 0; x < source.w(); ++x) {
                const int target_x = destination.x + x;
                const int target_y = destination.y + y;
                if (target_x < 0 || target_y < 0 ||
                    target_x >= target.w() || target_y >= target.h() ||
                    target_x < clip.x1() || target_y < clip.y1() ||
                    target_x >= clip.x2() || target_y >= clip.y2()) {
                    continue;
                }
                rgba &pixel =
                    target_pixels[target_y * target.w() + target_x];
                pixel =
                    blend(pixel, source.pixels()[y * source.w() + x]);
            }
        }
    }
} // namespace native::detail
