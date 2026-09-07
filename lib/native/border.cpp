//
// Draws independent outer frame edges with ordinary graphics primitives.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native/graphics.h>

namespace native
{
    gpx &gpx::draw_border(rect r, border_sides sides) {
        if (!r.w() || !r.h()) return *this;
        const point tl(r.x1(), r.y1()), tr(r.x2() - 1, r.y1());
        const point bl(r.x1(), r.y2() - 1), br(r.x2() - 1, r.y2() - 1);
        if (has_border(sides, border_sides::top)) draw_line(tl, tr);
        if (has_border(sides, border_sides::right)) draw_line(tr, br);
        if (has_border(sides, border_sides::bottom)) draw_line(bl, br);
        if (has_border(sides, border_sides::left)) draw_line(tl, bl);
        return *this;
    }
}
