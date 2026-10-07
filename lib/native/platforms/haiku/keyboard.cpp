//
// Maps Haiku raw reference-keyboard positions to portable Native key identities.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "keyboard.h"
namespace haiku
{
    native::key_code physical_key(int32 position) {
        switch (position) {
        case 1: return native::key_code::escape;
        case 17: return native::key_code::grave;
        case 27: return native::key_code::digit_0;
        case 28: return native::key_code::minus;
        case 29: return native::key_code::equals;
        case 30: return native::key_code::backspace;
        case 31: return native::key_code::insert;
        case 32: return native::key_code::home;
        case 33: return native::key_code::page_up;
        case 34: return native::key_code::num_lock;
        case 35: return native::key_code::keypad_divide;
        case 36: return native::key_code::keypad_multiply;
        case 37: return native::key_code::keypad_subtract;
        case 38: return native::key_code::tab;
        case 49: return native::key_code::left_bracket;
        case 50: return native::key_code::right_bracket;
        case 51: return native::key_code::backslash;
        case 52: return native::key_code::delete_key;
        case 53: return native::key_code::end;
        case 54: return native::key_code::page_down;
        case 55: return native::key_code::keypad_7;
        case 56: return native::key_code::keypad_8;
        case 57: return native::key_code::keypad_9;
        case 58: return native::key_code::keypad_add;
        case 59: return native::key_code::caps_lock;
        case 69: return native::key_code::semicolon;
        case 70: return native::key_code::apostrophe;
        case 71: return native::key_code::enter;
        case 72: return native::key_code::keypad_4;
        case 73: return native::key_code::keypad_5;
        case 74: return native::key_code::keypad_6;
        case 75: return native::key_code::left_shift;
        case 83: return native::key_code::comma;
        case 84: return native::key_code::period;
        case 85: return native::key_code::slash;
        case 86: return native::key_code::right_shift;
        case 87: return native::key_code::up;
        case 88: return native::key_code::keypad_1;
        case 89: return native::key_code::keypad_2;
        case 90: return native::key_code::keypad_3;
        case 91: return native::key_code::keypad_enter;
        case 92: return native::key_code::left_ctrl;
        case 93: return native::key_code::left_alt;
        case 94: return native::key_code::space;
        case 95: return native::key_code::right_alt;
        case 96: return native::key_code::right_ctrl;
        case 97: return native::key_code::left;
        case 98: return native::key_code::down;
        case 99: return native::key_code::right;
        case 100: return native::key_code::keypad_0;
        case 101: return native::key_code::keypad_decimal;
        case 102: return native::key_code::left_meta;
        case 103: return native::key_code::right_meta;
        case 2: return native::key_code::f1;
        case 3: return native::key_code::f2;
        case 4: return native::key_code::f3;
        case 5: return native::key_code::f4;
        case 6: return native::key_code::f5;
        case 7: return native::key_code::f6;
        case 8: return native::key_code::f7;
        case 9: return native::key_code::f8;
        case 10: return native::key_code::f9;
        case 11: return native::key_code::f10;
        case 12: return native::key_code::f11;
        case 13: return native::key_code::f12;
        case 18: return native::key_code::digit_1;
        case 19: return native::key_code::digit_2;
        case 20: return native::key_code::digit_3;
        case 21: return native::key_code::digit_4;
        case 22: return native::key_code::digit_5;
        case 23: return native::key_code::digit_6;
        case 24: return native::key_code::digit_7;
        case 25: return native::key_code::digit_8;
        case 26: return native::key_code::digit_9;
        case 39: return native::key_code::q;
        case 40: return native::key_code::w;
        case 41: return native::key_code::e;
        case 42: return native::key_code::r;
        case 43: return native::key_code::t;
        case 44: return native::key_code::y;
        case 45: return native::key_code::u;
        case 46: return native::key_code::i;
        case 47: return native::key_code::o;
        case 48: return native::key_code::p;
        case 60: return native::key_code::a;
        case 61: return native::key_code::s;
        case 62: return native::key_code::d;
        case 63: return native::key_code::f;
        case 64: return native::key_code::g;
        case 65: return native::key_code::h;
        case 66: return native::key_code::j;
        case 67: return native::key_code::k;
        case 68: return native::key_code::l;
        case 76: return native::key_code::z;
        case 77: return native::key_code::x;
        case 78: return native::key_code::c;
        case 79: return native::key_code::v;
        case 80: return native::key_code::b;
        case 81: return native::key_code::n;
        case 82: return native::key_code::m;
        default: return native::key_code::unknown;
        }
    }
}
