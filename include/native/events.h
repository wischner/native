//
// Declares input and paint event values emitted by public windows.
// Events carry backend-neutral coordinates and borrowed contexts.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include "geometry.h"
#include <string_view>

namespace native
{
    class gpx;

    // Physical positions on a US reference keyboard, never text or OS codes.
    enum class key_code
    {
        unknown,
        a,
        b,
        c,
        d,
        e,
        f,
        g,
        h,
        i,
        j,
        k,
        l,
        m,
        n,
        o,
        p,
        q,
        r,
        s,
        t,
        u,
        v,
        w,
        x,
        y,
        z,
        digit_0,
        digit_1,
        digit_2,
        digit_3,
        digit_4,
        digit_5,
        digit_6,
        digit_7,
        digit_8,
        digit_9,
        f1,
        f2,
        f3,
        f4,
        f5,
        f6,
        f7,
        f8,
        f9,
        f10,
        f11,
        f12,
        left_shift,
        right_shift,
        left_ctrl,
        right_ctrl,
        left_alt,
        right_alt,
        left_meta,
        right_meta,
        enter,
        keypad_enter,
        space,
        backspace,
        escape,
        tab,
        up,
        down,
        left,
        right,
        home,
        end,
        page_up,
        page_down,
        insert,
        delete_key,
        minus,
        equals,
        comma,
        period,
        slash,
        semicolon,
        apostrophe,
        left_bracket,
        right_bracket,
        backslash,
        grave,
        caps_lock,
        num_lock,
        scroll_lock,
        print_screen,
        pause,
        keypad_0,
        keypad_1,
        keypad_2,
        keypad_3,
        keypad_4,
        keypad_5,
        keypad_6,
        keypad_7,
        keypad_8,
        keypad_9,
        keypad_add,
        keypad_subtract,
        keypad_multiply,
        keypad_divide,
        keypad_decimal,
        count,
    };

    // Return the stable lowercase diagnostic name; unknown for invalid values.
    std::string_view key_name(key_code code);

    enum class key_action { press, release };

    // Post-transition modifier state; sided identity is in key_code.
    enum class key_modifiers : unsigned
    {
        none = 0, shift = 1, ctrl = 2, alt = 4, meta = 8,
        caps_lock = 16, num_lock = 32
    };

    // Combine portable modifier bits.
    constexpr key_modifiers operator|(key_modifiers a, key_modifiers b) {
        return static_cast<key_modifiers>(unsigned(a) | unsigned(b));
    }

    // Test portable modifier bits.
    constexpr key_modifiers operator&(key_modifiers a, key_modifiers b) {
        return static_cast<key_modifiers>(unsigned(a) & unsigned(b));
    }

    struct key_event
    {
        key_code key = key_code::unknown;
        key_action action = key_action::release;
        bool repeat = false;
        key_modifiers modifiers = key_modifiers::none;
    };



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
        mouse_event(mouse_button event_button,
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
        mouse_wheel_event(point event_position,
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
} // namespace native
