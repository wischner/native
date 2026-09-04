//
// Implements the backend-neutral part of the structural child
// container. Child registration, geometry, and layout are inherited
// from wnd, so only construction and destruction live here.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/panel.h>

#include <native/theme.h>

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

    void panel::on_native_paint(wnd_paint_event event) {
        auto appearance = theme::create(event.g);
        draw_background(
            event.g,
            *appearance,
            rect(point(0, 0), get_dimensions()),
            theme::state{});
        wnd::on_native_paint(event);
    }

    void panel::draw_background(
        gpx &,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_surface(bounds, surface_kind::panel, state);
    }
} // namespace native
