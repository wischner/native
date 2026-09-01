//
// Implements shared graphics-context drawing state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/graphics.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

#include "portable_font.h"
#include "text_util.h"

namespace
{
    native::rgba mix_pixel(const native::img &source,
                           const native::rect &source_rect,
                           double x,
                           double y,
                           native::image_filter filter) {
        const native::rgba *pixels = source.pixels();
        const auto pixel = [&](int px, int py) {
            return pixels[static_cast<std::size_t>(py) * source.w() +
                          static_cast<std::size_t>(px)];
        };
        if (filter == native::image_filter::nearest) {
            const int px = std::clamp(
                static_cast<int>(std::floor(x + 0.5)),
                static_cast<int>(source_rect.p.x),
                source_rect.x2() - 1);
            const int py = std::clamp(
                static_cast<int>(std::floor(y + 0.5)),
                static_cast<int>(source_rect.p.y),
                source_rect.y2() - 1);
            return pixel(px, py);
        }

        const int x0 = std::clamp(
            static_cast<int>(std::floor(x)),
            static_cast<int>(source_rect.p.x),
            source_rect.x2() - 1);
        const int y0 = std::clamp(
            static_cast<int>(std::floor(y)),
            static_cast<int>(source_rect.p.y),
            source_rect.y2() - 1);
        const int x1 = std::min(x0 + 1, source_rect.x2() - 1);
        const int y1 = std::min(y0 + 1, source_rect.y2() - 1);
        const double fx = x - std::floor(x);
        const double fy = y - std::floor(y);
        const native::rgba a = pixel(x0, y0);
        const native::rgba b = pixel(x1, y0);
        const native::rgba c = pixel(x0, y1);
        const native::rgba d = pixel(x1, y1);
        const auto channel = [&](std::uint8_t av,
                                 std::uint8_t bv,
                                 std::uint8_t cv,
                                 std::uint8_t dv) {
            const double top = av + (bv - av) * fx;
            const double bottom = cv + (dv - cv) * fx;
            return static_cast<std::uint8_t>(std::clamp(
                static_cast<int>(std::lround(
                    top + (bottom - top) * fy)),
                0,
                255));
        };
        return native::rgba(channel(a.r, b.r, c.r, d.r),
                            channel(a.g, b.g, c.g, d.g),
                            channel(a.b, b.b, c.b, d.b),
                            channel(a.a, b.a, c.a, d.a));
    }
} // namespace

namespace native
{
    gpx_state::gpx_state(gpx &graphics)
        : _graphics(&graphics)
        , _ink(graphics.get_ink())
        , _paper(graphics.get_paper())
        , _pen(graphics.get_pen())
        , _font(&graphics.get_font())
        , _clip(graphics.get_clip()) {}

    gpx_state::~gpx_state() {
        if (_graphics) {
            _graphics->set_ink(_ink)
                .set_paper(_paper)
                .set_pen(_pen)
                .set_font(*_font)
                .set_clip(_clip);
        }
    }

    gpx_state::gpx_state(gpx_state &&other) noexcept
        : _graphics(std::exchange(other._graphics, nullptr))
        , _ink(other._ink)
        , _paper(other._paper)
        , _pen(other._pen)
        , _font(other._font)
        , _clip(other._clip) {}

    gpx::~gpx() = default;

    gpx &gpx::set_ink(rgba c) {
        _ink = c;
        return *this;
    }

    rgba gpx::get_ink() const {
        return _ink;
    }

    gpx &gpx::set_paper(rgba c) {
        _paper = c;
        return *this;
    }

    rgba gpx::get_paper() const {
        return _paper;
    }

    gpx &gpx::set_pen(uint8_t t) {
        _thickness = t;
        return *this;
    }

    uint8_t gpx::get_pen() const {
        return _thickness;
    }

    gpx &gpx::set_font(const font_t &f) {
        _font = &f;
        return *this;
    }

    const font_t &gpx::get_font() const {
        if (_font)
            return *_font;
        return font_t::stock(font_role::system);
    }

    gpx_state gpx::save_state() {
        return gpx_state(*this);
    }

    font_metrics gpx::get_font_metrics() const {
        return get_font().get_metrics();
    }

    text_metrics gpx::measure_text(const std::string &text) const {
        return get_font().measure_text(text);
    }

    text_metrics gpx::measure_character(char32_t character) const {
        return get_font().measure_character(character);
    }

    gpx &gpx::draw_text(
        const std::string &text,
        point position) {
        const font_t &font = get_font();
        if (_font && !font.valid())
            return *this;
        if (!detail::is_portable_font(font.id()))
            return draw_native_text(text, position);

        detail::rasterized_text raster =
            detail::rasterize_portable_text(font.id(), text, _ink);
        if (!raster.image)
            return *this;
        const int x = static_cast<int>(position.x) + raster.offset.x;
        const int y = static_cast<int>(position.y) + raster.offset.y;
        if (x < std::numeric_limits<coord>::min() ||
            x > std::numeric_limits<coord>::max() ||
            y < std::numeric_limits<coord>::min() ||
            y > std::numeric_limits<coord>::max()) {
            return *this;
        }
        return draw_img(
            *raster.image,
            point(static_cast<coord>(x), static_cast<coord>(y)));
    }

    gpx &gpx::draw_ellipse(const rect &bounds, bool filled) {
        if (!bounds.d.w || !bounds.d.h)
            return *this;
        constexpr int segments = 48;
        std::vector<point> points;
        points.reserve(segments);
        const double center_x =
            bounds.p.x + (static_cast<double>(bounds.d.w) - 1.0) / 2.0;
        const double center_y =
            bounds.p.y + (static_cast<double>(bounds.d.h) - 1.0) / 2.0;
        const double radius_x =
            std::max(0.0, (static_cast<double>(bounds.d.w) - 1.0) / 2.0);
        const double radius_y =
            std::max(0.0, (static_cast<double>(bounds.d.h) - 1.0) / 2.0);
        constexpr double tau = 6.28318530717958647692;
        for (int index = 0; index < segments; ++index) {
            const double angle = tau * index / segments;
            points.emplace_back(
                static_cast<coord>(std::lround(
                    center_x + std::cos(angle) * radius_x)),
                static_cast<coord>(std::lround(
                    center_y + std::sin(angle) * radius_y)));
        }
        return draw_polygon(points, filled);
    }

    gpx &gpx::draw_polyline(const std::vector<point> &points) {
        for (std::size_t index = 1; index < points.size(); ++index)
            draw_line(points[index - 1], points[index]);
        return *this;
    }

    gpx &gpx::draw_polygon(const std::vector<point> &points,
                           bool filled) {
        if (points.size() < 2)
            return *this;
        if (!filled) {
            draw_polyline(points);
            draw_line(points.back(), points.front());
            return *this;
        }

        int minimum_y = points.front().y;
        int maximum_y = points.front().y;
        for (const point &value : points) {
            minimum_y = std::min(minimum_y, static_cast<int>(value.y));
            maximum_y = std::max(maximum_y, static_cast<int>(value.y));
        }
        for (int y = minimum_y; y <= maximum_y; ++y) {
            std::vector<int> intersections;
            for (std::size_t current = 0; current < points.size();
                 ++current) {
                const point a = points[current];
                const point b = points[(current + 1) % points.size()];
                if ((a.y <= y && b.y > y) ||
                    (b.y <= y && a.y > y)) {
                    const double x = a.x +
                        (static_cast<double>(y - a.y) *
                         (b.x - a.x)) /
                            (b.y - a.y);
                    intersections.push_back(
                        static_cast<int>(std::lround(x)));
                }
            }
            std::sort(intersections.begin(), intersections.end());
            for (std::size_t index = 1;
                 index < intersections.size();
                 index += 2) {
                draw_line(point(static_cast<coord>(
                                    intersections[index - 1]),
                                static_cast<coord>(y)),
                          point(static_cast<coord>(intersections[index]),
                                static_cast<coord>(y)));
            }
        }
        return *this;
    }

    gpx &gpx::draw_img(const img &source,
                       const rect &destination,
                       image_filter filter) {
        return draw_img(source,
                        rect(0, 0, source.w(), source.h()),
                        destination,
                        filter);
    }

    gpx &gpx::draw_img(const img &source,
                       const rect &source_rect,
                       const rect &destination,
                       image_filter filter) {
        if (source_rect.p.x < 0 || source_rect.p.y < 0 ||
            source_rect.x2() > source.w() ||
            source_rect.y2() > source.h()) {
            throw std::out_of_range(
                "gpx image source rectangle is out of range");
        }
        if (!source_rect.d.w || !source_rect.d.h ||
            !destination.d.w || !destination.d.h) {
            return *this;
        }
        if (source_rect.p.x == 0 && source_rect.p.y == 0 &&
            source_rect.d.w == source.w() &&
            source_rect.d.h == source.h() &&
            destination.d.w == source.w() &&
            destination.d.h == source.h()) {
            return draw_img(source, destination.p);
        }
        img resized(destination.d.w, destination.d.h);
        rgba *target = resized.pixels();
        const double scale_x =
            static_cast<double>(source_rect.d.w) / destination.d.w;
        const double scale_y =
            static_cast<double>(source_rect.d.h) / destination.d.h;
        for (int y = 0; y < destination.d.h; ++y) {
            const double source_y = source_rect.p.y +
                (static_cast<double>(y) + 0.5) * scale_y - 0.5;
            for (int x = 0; x < destination.d.w; ++x) {
                const double source_x = source_rect.p.x +
                    (static_cast<double>(x) + 0.5) * scale_x - 0.5;
                target[static_cast<std::size_t>(y) * destination.d.w +
                       static_cast<std::size_t>(x)] =
                    mix_pixel(source,
                              source_rect,
                              source_x,
                              source_y,
                              filter);
            }
        }
        return draw_img(resized, destination.p);
    }

    gpx &gpx::draw_text(const std::string &text,
                        const rect &bounds,
                        const text_layout &layout) {
        if (!bounds.d.w || !bounds.d.h)
            return *this;
        auto saved = save_state();
        set_clip(get_clip().intersect(bounds));
        std::string display = text;
        text_metrics measured = measure_text(display);
        if (layout.overflow == text_overflow::ellipsis &&
            measured.width > bounds.d.w) {
            constexpr const char ellipsis[] = "\xe2\x80\xa6";
            std::size_t offset = display.size();
            do {
                offset = detail::previous_utf8(display, offset);
                display.resize(offset);
                measured = measure_text(display + ellipsis);
            } while (!display.empty() &&
                     measured.width > bounds.d.w);
            display += ellipsis;
        }
        measured = measure_text(display);
        int x = bounds.p.x;
        if (layout.horizontal == text_align::center)
            x += (static_cast<int>(bounds.d.w) - measured.width) / 2;
        else if (layout.horizontal == text_align::end)
            x += static_cast<int>(bounds.d.w) - measured.width;
        int y = bounds.p.y;
        if (layout.vertical == text_valign::center)
            y += (static_cast<int>(bounds.d.h) - measured.height) / 2;
        else if (layout.vertical == text_valign::bottom)
            y += static_cast<int>(bounds.d.h) - measured.height;
        return draw_text(display,
                         point(static_cast<coord>(x),
                               static_cast<coord>(y)));
    }

} // namespace native
