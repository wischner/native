//
// Maps Apple physical virtual keys to layout-independent Native positions.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "keyboard.h"
#include <IOKit/hidsystem/IOLLEvent.h>
namespace mac
{
    native::key_event physical_key(NSEvent *event, native::key_action action) {
        native::key_code code = native::key_code::unknown;
        switch ([event keyCode]) {
        case 0: code = native::key_code::a; break;
        case 1: code = native::key_code::s; break;
        case 2: code = native::key_code::d; break;
        case 3: code = native::key_code::f; break;
        case 4: code = native::key_code::h; break;
        case 5: code = native::key_code::g; break;
        case 6: code = native::key_code::z; break;
        case 7: code = native::key_code::x; break;
        case 8: code = native::key_code::c; break;
        case 9: code = native::key_code::v; break;
        case 11: code = native::key_code::b; break;
        case 12: code = native::key_code::q; break;
        case 13: code = native::key_code::w; break;
        case 14: code = native::key_code::e; break;
        case 15: code = native::key_code::r; break;
        case 16: code = native::key_code::y; break;
        case 17: code = native::key_code::t; break;
        case 18: code = native::key_code::digit_1; break;
        case 19: code = native::key_code::digit_2; break;
        case 20: code = native::key_code::digit_3; break;
        case 21: code = native::key_code::digit_4; break;
        case 22: code = native::key_code::digit_6; break;
        case 23: code = native::key_code::digit_5; break;
        case 24: code = native::key_code::equals; break;
        case 25: code = native::key_code::digit_9; break;
        case 26: code = native::key_code::digit_7; break;
        case 27: code = native::key_code::minus; break;
        case 28: code = native::key_code::digit_8; break;
        case 29: code = native::key_code::digit_0; break;
        case 30: code = native::key_code::right_bracket; break;
        case 31: code = native::key_code::o; break;
        case 32: code = native::key_code::u; break;
        case 33: code = native::key_code::left_bracket; break;
        case 34: code = native::key_code::i; break;
        case 35: code = native::key_code::p; break;
        case 36: code = native::key_code::enter; break;
        case 37: code = native::key_code::l; break;
        case 38: code = native::key_code::j; break;
        case 39: code = native::key_code::apostrophe; break;
        case 40: code = native::key_code::k; break;
        case 41: code = native::key_code::semicolon; break;
        case 42: code = native::key_code::backslash; break;
        case 43: code = native::key_code::comma; break;
        case 44: code = native::key_code::slash; break;
        case 45: code = native::key_code::n; break;
        case 46: code = native::key_code::m; break;
        case 47: code = native::key_code::period; break;
        case 48: code = native::key_code::tab; break;
        case 49: code = native::key_code::space; break;
        case 50: code = native::key_code::grave; break;
        case 51: code = native::key_code::backspace; break;
        case 53: code = native::key_code::escape; break;
        case 54: code = native::key_code::right_meta; break;
        case 55: code = native::key_code::left_meta; break;
        case 56: code = native::key_code::left_shift; break;
        case 57: code = native::key_code::caps_lock; break;
        case 58: code = native::key_code::left_alt; break;
        case 59: code = native::key_code::left_ctrl; break;
        case 60: code = native::key_code::right_shift; break;
        case 61: code = native::key_code::right_alt; break;
        case 62: code = native::key_code::right_ctrl; break;
        case 65: code = native::key_code::keypad_decimal; break;
        case 67: code = native::key_code::keypad_multiply; break;
        case 69: code = native::key_code::keypad_add; break;
        case 71: code = native::key_code::num_lock; break;
        case 75: code = native::key_code::keypad_divide; break;
        case 76: code = native::key_code::keypad_enter; break;
        case 78: code = native::key_code::keypad_subtract; break;
        case 82: code = native::key_code::keypad_0; break;
        case 83: code = native::key_code::keypad_1; break;
        case 84: code = native::key_code::keypad_2; break;
        case 85: code = native::key_code::keypad_3; break;
        case 86: code = native::key_code::keypad_4; break;
        case 87: code = native::key_code::keypad_5; break;
        case 88: code = native::key_code::keypad_6; break;
        case 89: code = native::key_code::keypad_7; break;
        case 91: code = native::key_code::keypad_8; break;
        case 92: code = native::key_code::keypad_9; break;
        case 96: code = native::key_code::f5; break;
        case 97: code = native::key_code::f6; break;
        case 98: code = native::key_code::f7; break;
        case 99: code = native::key_code::f3; break;
        case 100: code = native::key_code::f8; break;
        case 101: code = native::key_code::f9; break;
        case 103: code = native::key_code::f11; break;
        case 109: code = native::key_code::f10; break;
        case 111: code = native::key_code::f12; break;
        case 114: code = native::key_code::insert; break;
        case 115: code = native::key_code::home; break;
        case 116: code = native::key_code::page_up; break;
        case 117: code = native::key_code::delete_key; break;
        case 118: code = native::key_code::f4; break;
        case 119: code = native::key_code::end; break;
        case 120: code = native::key_code::f2; break;
        case 121: code = native::key_code::page_down; break;
        case 122: code = native::key_code::f1; break;
        case 123: code = native::key_code::left; break;
        case 124: code = native::key_code::right; break;
        case 125: code = native::key_code::down; break;
        case 126: code = native::key_code::up; break;
        }
        const auto flags = [event modifierFlags];
        unsigned mods = 0;
        if (flags & NSEventModifierFlagShift) mods |= 1;
        if (flags & NSEventModifierFlagControl) mods |= 2;
        if (flags & NSEventModifierFlagOption) mods |= 4;
        if (flags & NSEventModifierFlagCommand) mods |= 8;
        if (flags & NSEventModifierFlagCapsLock) mods |= 16;
        if ([event type] == NSEventTypeFlagsChanged) {
            NSUInteger mask = 0;
            switch (code) {
            case native::key_code::left_shift: mask = NX_DEVICELSHIFTKEYMASK; break;
            case native::key_code::right_shift: mask = NX_DEVICERSHIFTKEYMASK; break;
            case native::key_code::left_ctrl: mask = NX_DEVICELCTLKEYMASK; break;
            case native::key_code::right_ctrl: mask = NX_DEVICERCTLKEYMASK; break;
            case native::key_code::left_alt: mask = NX_DEVICELALTKEYMASK; break;
            case native::key_code::right_alt: mask = NX_DEVICERALTKEYMASK; break;
            case native::key_code::left_meta: mask = NX_DEVICELCMDKEYMASK; break;
            case native::key_code::right_meta: mask = NX_DEVICERCMDKEYMASK; break;
            case native::key_code::caps_lock: mask = NSEventModifierFlagCapsLock; break;
            default: break;
            }
            action = ([event modifierFlags] & mask) ? native::key_action::press :
                native::key_action::release;
        }
        return {code, action, [event type] == NSEventTypeKeyDown && [event isARepeat],
            static_cast<native::key_modifiers>(mods)};
    }
}
