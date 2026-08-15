//
// Implements construction of backend-neutral input and paint events.
// Event values borrow graphics contexts but own all scalar event data.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/events.h>

namespace native
{
    mouse_event::mouse_event() = default;

    mouse_event::mouse_event(
        mouse_button event_button,
        mouse_action event_action,
        point event_position)
        : button(event_button),
          action(event_action),
          position(event_position) {
    }

    mouse_wheel_event::mouse_wheel_event() = default;

    mouse_wheel_event::mouse_wheel_event(
        point event_position,
        coord event_delta,
        wheel_direction event_direction)
        : position(event_position),
          delta(event_delta),
          direction(event_direction) {
    }

    wnd_paint_event::wnd_paint_event(
        const rect &invalid,
        gpx &graphics)
        : r(invalid), g(graphics) {
    }
}
