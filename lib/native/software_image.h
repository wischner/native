//
// Implements target-independent RGBA image drawing primitives.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <algorithm>
#include <cstdlib>

#include <native/graphics.h>

namespace native::detail
{
    inline rect image_bounds(const img &target) {
        return rect(0, 0, target.w(), target.h());
    }

    inline void put_image_pixel(
        const img &target,
        const rect &clip,
        int x,
        int y,
        rgba color) {
        if (x < 0 || y < 0 || x >= target.w() || y >= target.h() ||
            x < clip.x1() || y < clip.y1() ||
            x >= clip.x2() || y >= clip.y2()) {
            return;
        }
        const_cast<rgba *>(target.pixels())[y * target.w() + x] = color;
    }

    inline void clear_image(
        const img &target,
        const rect &clip,
        rgba color) {
        const rect area = clip.intersect(image_bounds(target));
        rgba *pixels = const_cast<rgba *>(target.pixels());
        for (int y = area.y1(); y < area.y2(); ++y) {
            for (int x = area.x1(); x < area.x2(); ++x)
                pixels[y * target.w() + x] = color;
        }
    }

    inline void draw_image_line(
        const img &target,
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
                for (int offset_x = first; offset_x <= last; ++offset_x) {
                    put_image_pixel(
                        target,
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

    inline void draw_image_rect(
        const img &target,
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
        draw_image_line(
            target, clip, bounds.p, point(right, bounds.p.y),
            color, thickness);
        draw_image_line(
            target, clip, bounds.p, point(bounds.p.x, bottom),
            color, thickness);
        draw_image_line(
            target, clip, point(right, bounds.p.y), point(right, bottom),
            color, thickness);
        draw_image_line(
            target, clip, point(bounds.p.x, bottom), point(right, bottom),
            color, thickness);
    }

    inline void copy_image(
        const img &target,
        const rect &clip,
        const img &source,
        point destination) {
        for (int y = 0; y < source.h(); ++y) {
            for (int x = 0; x < source.w(); ++x) {
                put_image_pixel(
                    target,
                    clip,
                    destination.x + x,
                    destination.y + y,
                    source.pixels()[y * source.w() + x]);
            }
        }
    }
}
