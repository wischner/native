//
// Translates private USB and PC positions and manages focused input trees.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "input_state.h"

namespace native::detail
{
    wnd *input_target(wnd &root) {
        for (wnd *child : root._children) {
            if (!child || !child->get_created() || !child->get_visible())
                continue;
            if (auto *nested = input_target(*child)) return nested;
        }
        auto *state = peer_state<input_state>(root);
        return state && state->focused ? &root : nullptr;
    }

    void reset_input(wnd &root) {
        // Weak lifetime snapshots avoid retaining a dead child across callbacks.
        const std::weak_ptr<wnd_lifetime> owner = root._lifetime;
        const bool tracked = static_cast<bool>(root._lifetime);
        std::vector<std::pair<wnd *, std::weak_ptr<wnd_lifetime>>> children;
        for (wnd *child : root._children)
            if (child && child->_lifetime) children.push_back({child, child->_lifetime});
        for (const auto &[child, lifetime] : children)
            if (!lifetime.expired()) reset_input(*child);
        if (!tracked || !owner.expired()) root.on_native_key_reset();
    }

    bool key_held(wnd &target, key_code key) {
        auto *state = peer_state<input_state>(target);
        const auto index = static_cast<unsigned>(key);
        return state && index < static_cast<unsigned>(key_code::count)
            && state->held.test(index);
    }

    key_code usb_key(unsigned position) {
        if (position >= 4 && position <= 29)
            return static_cast<key_code>(unsigned(key_code::a) + position - 4);
        if (position >= 30 && position <= 38)
            return static_cast<key_code>(unsigned(key_code::digit_1) + position - 30);
        if (position >= 58 && position <= 69)
            return static_cast<key_code>(unsigned(key_code::f1) + position - 58);
        switch (position) {
        case 39: return key_code::digit_0;
        case 40: return key_code::enter;
        case 41: return key_code::escape;
        case 42: return key_code::backspace;
        case 43: return key_code::tab;
        case 44: return key_code::space;
        case 45: return key_code::minus;
        case 46: return key_code::equals;
        case 47: return key_code::left_bracket;
        case 48: return key_code::right_bracket;
        case 49: return key_code::backslash;
        case 51: return key_code::semicolon;
        case 52: return key_code::apostrophe;
        case 53: return key_code::grave;
        case 54: return key_code::comma;
        case 55: return key_code::period;
        case 56: return key_code::slash;
        case 57: return key_code::caps_lock;
        case 70: return key_code::print_screen;
        case 71: return key_code::scroll_lock;
        case 72: return key_code::pause;
        case 73: return key_code::insert;
        case 74: return key_code::home;
        case 75: return key_code::page_up;
        case 76: return key_code::delete_key;
        case 77: return key_code::end;
        case 78: return key_code::page_down;
        case 79: return key_code::right;
        case 80: return key_code::left;
        case 81: return key_code::down;
        case 82: return key_code::up;
        case 83: return key_code::num_lock;
        case 84: return key_code::keypad_divide;
        case 85: return key_code::keypad_multiply;
        case 86: return key_code::keypad_subtract;
        case 87: return key_code::keypad_add;
        case 88: return key_code::keypad_enter;
        case 89: return key_code::keypad_1;
        case 90: return key_code::keypad_2;
        case 91: return key_code::keypad_3;
        case 92: return key_code::keypad_4;
        case 93: return key_code::keypad_5;
        case 94: return key_code::keypad_6;
        case 95: return key_code::keypad_7;
        case 96: return key_code::keypad_8;
        case 97: return key_code::keypad_9;
        case 98: return key_code::keypad_0;
        case 99: return key_code::keypad_decimal;
        case 224: return key_code::left_ctrl;
        case 225: return key_code::left_shift;
        case 226: return key_code::left_alt;
        case 227: return key_code::left_meta;
        case 228: return key_code::right_ctrl;
        case 229: return key_code::right_shift;
        case 230: return key_code::right_alt;
        case 231: return key_code::right_meta;
        default: return key_code::unknown;
        }
    }

    key_code pc_key(unsigned scan, bool extended) {
        if (extended) {
            switch (scan) {
            case 28: return key_code::keypad_enter;
            case 29: return key_code::right_ctrl;
            case 56: return key_code::right_alt;
            case 71: return key_code::home;
            case 72: return key_code::up;
            case 73: return key_code::page_up;
            case 75: return key_code::left;
            case 77: return key_code::right;
            case 79: return key_code::end;
            case 80: return key_code::down;
            case 81: return key_code::page_down;
            case 82: return key_code::insert;
            case 83: return key_code::delete_key;
            case 91: return key_code::left_meta;
            case 92: return key_code::right_meta;
            case 53: return key_code::keypad_divide;
            case 55: return key_code::print_screen;
            default: return key_code::unknown;
            }
        }
        switch (scan) {
        case 1: return key_code::escape;
        case 11: return key_code::digit_0;
        case 12: return key_code::minus;
        case 13: return key_code::equals;
        case 14: return key_code::backspace;
        case 15: return key_code::tab;
        case 26: return key_code::left_bracket;
        case 27: return key_code::right_bracket;
        case 28: return key_code::enter;
        case 29: return key_code::left_ctrl;
        case 39: return key_code::semicolon;
        case 40: return key_code::apostrophe;
        case 41: return key_code::grave;
        case 42: return key_code::left_shift;
        case 43: return key_code::backslash;
        case 51: return key_code::comma;
        case 52: return key_code::period;
        case 53: return key_code::slash;
        case 54: return key_code::right_shift;
        case 55: return key_code::keypad_multiply;
        case 56: return key_code::left_alt;
        case 57: return key_code::space;
        case 58: return key_code::caps_lock;
        case 69: return key_code::num_lock;
        case 70: return key_code::scroll_lock;
        case 71: return key_code::keypad_7;
        case 72: return key_code::keypad_8;
        case 73: return key_code::keypad_9;
        case 74: return key_code::keypad_subtract;
        case 75: return key_code::keypad_4;
        case 76: return key_code::keypad_5;
        case 77: return key_code::keypad_6;
        case 78: return key_code::keypad_add;
        case 79: return key_code::keypad_1;
        case 80: return key_code::keypad_2;
        case 81: return key_code::keypad_3;
        case 82: return key_code::keypad_0;
        case 83: return key_code::keypad_decimal;
        case 87: return key_code::f11;
        case 88: return key_code::f12;
        case 2: return key_code::digit_1;
        case 3: return key_code::digit_2;
        case 4: return key_code::digit_3;
        case 5: return key_code::digit_4;
        case 6: return key_code::digit_5;
        case 7: return key_code::digit_6;
        case 8: return key_code::digit_7;
        case 9: return key_code::digit_8;
        case 10: return key_code::digit_9;
        case 16: return key_code::q;
        case 17: return key_code::w;
        case 18: return key_code::e;
        case 19: return key_code::r;
        case 20: return key_code::t;
        case 21: return key_code::y;
        case 22: return key_code::u;
        case 23: return key_code::i;
        case 24: return key_code::o;
        case 25: return key_code::p;
        case 30: return key_code::a;
        case 31: return key_code::s;
        case 32: return key_code::d;
        case 33: return key_code::f;
        case 34: return key_code::g;
        case 35: return key_code::h;
        case 36: return key_code::j;
        case 37: return key_code::k;
        case 38: return key_code::l;
        case 44: return key_code::z;
        case 45: return key_code::x;
        case 46: return key_code::c;
        case 47: return key_code::v;
        case 48: return key_code::b;
        case 49: return key_code::n;
        case 50: return key_code::m;
        case 59: return key_code::f1;
        case 60: return key_code::f2;
        case 61: return key_code::f3;
        case 62: return key_code::f4;
        case 63: return key_code::f5;
        case 64: return key_code::f6;
        case 65: return key_code::f7;
        case 66: return key_code::f8;
        case 67: return key_code::f9;
        case 68: return key_code::f10;
        default: return key_code::unknown;
        }
    }
}
