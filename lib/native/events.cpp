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

    mouse_event::mouse_event(mouse_button event_button,
                             mouse_action event_action,
                             point event_position)
        : button(event_button)
        , action(event_action)
        , position(event_position) {}

    mouse_wheel_event::mouse_wheel_event() = default;

    mouse_wheel_event::mouse_wheel_event(
        point event_position,
        coord event_delta,
        wheel_direction event_direction)
        : position(event_position)
        , delta(event_delta)
        , direction(event_direction) {}

    wnd_paint_event::wnd_paint_event(const rect &invalid, gpx &graphics)
        : r(invalid)
        , g(graphics) {}
} // namespace native

namespace native
{
    std::string_view key_name(key_code code) {
        static constexpr std::string_view names[] = {
            "unknown",
            "a",
            "b",
            "c",
            "d",
            "e",
            "f",
            "g",
            "h",
            "i",
            "j",
            "k",
            "l",
            "m",
            "n",
            "o",
            "p",
            "q",
            "r",
            "s",
            "t",
            "u",
            "v",
            "w",
            "x",
            "y",
            "z",
            "digit_0",
            "digit_1",
            "digit_2",
            "digit_3",
            "digit_4",
            "digit_5",
            "digit_6",
            "digit_7",
            "digit_8",
            "digit_9",
            "f1",
            "f2",
            "f3",
            "f4",
            "f5",
            "f6",
            "f7",
            "f8",
            "f9",
            "f10",
            "f11",
            "f12",
            "left_shift",
            "right_shift",
            "left_ctrl",
            "right_ctrl",
            "left_alt",
            "right_alt",
            "left_meta",
            "right_meta",
            "enter",
            "keypad_enter",
            "space",
            "backspace",
            "escape",
            "tab",
            "up",
            "down",
            "left",
            "right",
            "home",
            "end",
            "page_up",
            "page_down",
            "insert",
            "delete_key",
            "minus",
            "equals",
            "comma",
            "period",
            "slash",
            "semicolon",
            "apostrophe",
            "left_bracket",
            "right_bracket",
            "backslash",
            "grave",
            "caps_lock",
            "num_lock",
            "scroll_lock",
            "print_screen",
            "pause",
            "keypad_0",
            "keypad_1",
            "keypad_2",
            "keypad_3",
            "keypad_4",
            "keypad_5",
            "keypad_6",
            "keypad_7",
            "keypad_8",
            "keypad_9",
            "keypad_add",
            "keypad_subtract",
            "keypad_multiply",
            "keypad_divide",
            "keypad_decimal",
        };
        const auto index = static_cast<unsigned>(code);
        return index < sizeof(names) / sizeof(names[0]) ? names[index] : "unknown";
    }
}
