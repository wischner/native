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
#include <cmath>

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
    rgba sample_image(const img &source, const rect &bounds,
                      double x, double y, image_filter filter) {
        x = std::clamp(x, double(bounds.x1()), double(bounds.x2() - 1));
        y = std::clamp(y, double(bounds.y1()), double(bounds.y2() - 1));
        const auto pixel = [&](int px, int py) {
            return source.pixels()[std::size_t(py) * source.w() + px];
        };
        if (filter == image_filter::nearest)
            return pixel(int(std::floor(x + 0.5)), int(std::floor(y + 0.5)));
        const int x0 = int(std::floor(x)), y0 = int(std::floor(y));
        const int x1 = std::min(x0 + 1, int(bounds.x2() - 1));
        const int y1 = std::min(y0 + 1, int(bounds.y2() - 1));
        const double fx = x - x0, fy = y - y0;
        const rgba samples[] = {pixel(x0, y0), pixel(x1, y0),
                                pixel(x0, y1), pixel(x1, y1)};
        const double weights[] = {(1 - fx) * (1 - fy), fx * (1 - fy),
                                   (1 - fx) * fy, fx * fy};
        double alpha = 0, red = 0, green = 0, blue = 0;
        for (int i = 0; i < 4; ++i) {
            const double weight = weights[i] * samples[i].a;
            alpha += weight;
            red += weight * samples[i].r;
            green += weight * samples[i].g;
            blue += weight * samples[i].b;
        }
        if (alpha == 0) return rgba(0, 0, 0, 0);
        const auto byte = [](double value) {
            return static_cast<std::uint8_t>(std::clamp(
                std::lround(value), 0L, 255L));
        };
        return rgba(byte(red / alpha), byte(green / alpha),
                    byte(blue / alpha), byte(alpha));
    }

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
        const auto area = rect(destination, size(source.w(), source.h()))
            .intersect(clip).intersect(image_bounds(target));
        for (int y = area.y1(); y < area.y2(); ++y) {
            for (int x = area.x1(); x < area.x2(); ++x) {
                const rgba incoming = source.pixels()[
                    (y - destination.y) * source.w() + x - destination.x];
                rgba &pixel = target_pixels[y * target.w() + x];
                pixel = incoming.a == 255 ? incoming : blend(pixel, incoming);
            }
        }
    }
} // namespace native::detail
