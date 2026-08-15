//
// Declares input and paint event values emitted by public windows.
// Events carry backend-neutral coordinates and borrowed contexts.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include "geometry.h"

namespace native
{
    class gpx;

    // Identifies a mouse button independently of the native toolkit.
    enum class mouse_button
    {
        none = 0,
        left,
        right,
        middle,
        x1,
        x2
    };

    // Identifies whether a mouse button was pressed or released.
    enum class mouse_action
    {
        press,
        release
    };

    // Describes a mouse-button event in client coordinates.
    struct mouse_event
    {
        mouse_button button = mouse_button::none;
        mouse_action action = mouse_action::release;
        point position;

        // Construct an empty mouse event.
        mouse_event();

        // Construct a mouse event from button, action, and position.
        mouse_event(
            mouse_button event_button,
            mouse_action event_action,
            point event_position);
    };

    // Identifies the axis affected by a wheel event.
    enum class wheel_direction
    {
        vertical,
        horizontal
    };

    // Describes a mouse-wheel event in client coordinates.
    struct mouse_wheel_event
    {
        // Mouse position at the time of scrolling, when available.
        point position;

        // Positive means up/right; negative means down/left.
        coord delta = 0;

        wheel_direction direction = wheel_direction::vertical;

        // Construct an empty vertical wheel event.
        mouse_wheel_event();

        // Construct a wheel event from its position, delta, and axis.
        mouse_wheel_event(
            point event_position,
            coord event_delta,
            wheel_direction event_direction);
    };

    // Provides the invalid rectangle and borrowed context for painting.
    struct wnd_paint_event
    {
        rect r;
        gpx &g;

        //
        // Construct a window paint event.
        //
        // Parameters:
        //      invalid     - Region requiring repainting.
        //      graphics    - Borrowed drawing context for the window.
        //
        wnd_paint_event(const rect &invalid, gpx &graphics);
    };
}
