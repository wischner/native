#include <native.h>

namespace native
{
    // Point
    point::point(coord x_, coord y_) : x(x_), y(y_) {}

    // Line
    line::line(point p1, point p2) : a(p1), b(p2) {}
    line::line(coord x1, coord y1, coord x2, coord y2) : a(x1, y1), b(x2, y2) {}
    bool line::contains(point pt) const
    {
        coord dx1 = b.x - a.x;
        coord dy1 = b.y - a.y;
        coord dx2 = pt.x - a.x;
        coord dy2 = pt.y - a.y;

        // Check if cross product is zero (colinear)
        if (dx1 * dy2 != dx2 * dy1)
            return false;

        // Bounding box check
        coord min_x = std::min(a.x, b.x), max_x = std::max(a.x, b.x);
        coord min_y = std::min(a.y, b.y), max_y = std::max(a.y, b.y);

        return (pt.x >= min_x && pt.x <= max_x &&
                pt.y >= min_y && pt.y <= max_y);
    }

    // Size
    size::size(dim w_, dim h_) : w(w_), h(h_) {}

    // Rect
    rect::rect(point p_, size d_) : p(p_), d(d_) {}
    rect::rect(coord x, coord y, dim w, dim h) : p(x, y), d(w, h) {}

    coord rect::x1() const { return p.x; }
    coord rect::y1() const { return p.y; }
    coord rect::x2() const { return p.x + d.w; }
    coord rect::y2() const { return p.y + d.h; }

    dim rect::w() const { return d.w; }
    dim rect::h() const { return d.h; }

    bool rect::contains(point pt) const
    {
        return pt.x >= x1() && pt.x < x2() && pt.y >= y1() && pt.y < y2();
    }

    rect rect::intersect(const rect &other) const
    {
        coord nx1 = std::max(p.x, other.p.x);
        coord ny1 = std::max(p.y, other.p.y);
        coord nx2 = std::min(x2(), other.x2());
        coord ny2 = std::min(y2(), other.y2());

        if (nx2 <= nx1 || ny2 <= ny1)
            return rect(); // empty / no intersection

        return rect(nx1, ny1, nx2 - nx1, ny2 - ny1);
    }
} // namespace native
