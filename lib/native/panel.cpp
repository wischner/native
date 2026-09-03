//
// Implements the backend-neutral part of the structural child
// container. Child registration, geometry, and layout are inherited
// from wnd, so only construction and destruction live here.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/panel.h>

namespace native
{
    panel::panel(coord x, coord y, dim width, dim height)
        : wnd(x, y, width, height) {}

    panel::panel(const point &position, const size &dimensions)
        : wnd(position, dimensions) {}

    panel::panel(const rect &bounds)
        : wnd(bounds) {}

    panel::~panel() {
        destroy();
    }
} // namespace native
