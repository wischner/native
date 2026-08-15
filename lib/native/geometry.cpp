//
// Implements backend-neutral geometry calculations.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>

#include <native/geometry.h>

namespace native
{
    point::point() = default;

    point::point(coord x_value, coord y_value)
        : x(x_value), y(y_value) {
    }

    line::line() = default;

    line::line(point first, point second)
        : a(first), b(second) {
    }

    line::line(coord x1, coord y1, coord x2, coord y2)
        : a(x1, y1), b(x2, y2) {
    }

    bool line::contains(point candidate) const {
        const coord line_dx = b.x - a.x;
        const coord line_dy = b.y - a.y;
        const coord point_dx = candidate.x - a.x;
        const coord point_dy = candidate.y - a.y;

        // A zero cross product identifies collinear points.
        if (line_dx * point_dy != point_dx * line_dy)
            return false;

        const coord min_x = std::min(a.x, b.x);
        const coord max_x = std::max(a.x, b.x);
        const coord min_y = std::min(a.y, b.y);
        const coord max_y = std::max(a.y, b.y);

        return candidate.x >= min_x && candidate.x <= max_x &&
               candidate.y >= min_y && candidate.y <= max_y;
    }

    size::size() = default;

    size::size(dim width, dim height)
        : w(width), h(height) {
    }

    rect::rect() = default;

    rect::rect(point origin, size dimensions)
        : p(origin), d(dimensions) {
    }

    rect::rect(coord x, coord y, dim width, dim height)
        : p(x, y), d(width, height) {
    }

    coord rect::x1() const { return p.x; }
    coord rect::y1() const { return p.y; }
    coord rect::x2() const { return p.x + d.w; }
    coord rect::y2() const { return p.y + d.h; }

    dim rect::w() const { return d.w; }
    dim rect::h() const { return d.h; }

    bool rect::contains(point candidate) const {
        return candidate.x >= x1() && candidate.x < x2() &&
               candidate.y >= y1() && candidate.y < y2();
    }

    rect rect::intersect(const rect &other) const {
        const coord overlap_x1 = std::max(p.x, other.p.x);
        const coord overlap_y1 = std::max(p.y, other.p.y);
        const coord overlap_x2 = std::min(x2(), other.x2());
        const coord overlap_y2 = std::min(y2(), other.y2());

        if (overlap_x2 <= overlap_x1 ||
            overlap_y2 <= overlap_y1)
            return rect();

        return rect(
            overlap_x1,
            overlap_y1,
            overlap_x2 - overlap_x1,
            overlap_y2 - overlap_y1);
    }
} // namespace native
