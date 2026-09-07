//
// Masks unselected edges of native X widgets using the Shape extension.
// Widget state, geometry, rendering, and input remain toolkit-owned.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "x_border.h"
#include <algorithm>
#include <X11/extensions/shape.h>

namespace native::detail
{
    void shape_border(Display *display, Window window, border_sides sides,
                      int inside, int outside) {
        if (!display || !window) return;
        if (sides == border_sides::all) {
            XShapeCombineMask(display, window, ShapeBounding, 0, 0, None, ShapeSet);
            return;
        }
        XWindowAttributes attributes{};
        if (!XGetWindowAttributes(display, window, &attributes)) return;
        const int left = has_border(sides, border_sides::left) ? -outside : inside;
        const int top = has_border(sides, border_sides::top) ? -outside : inside;
        const int right = attributes.width +
            (has_border(sides, border_sides::right) ? outside : -inside);
        const int bottom = attributes.height +
            (has_border(sides, border_sides::bottom) ? outside : -inside);
        XRectangle area{static_cast<short>(left), static_cast<short>(top),
            static_cast<unsigned short>(std::max(0, right - left)),
            static_cast<unsigned short>(std::max(0, bottom - top))};
        XShapeCombineRectangles(display, window, ShapeBounding, 0, 0,
                               &area, 1, ShapeSet, Unsorted);
    }
}
