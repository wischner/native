//
// Uses XKB physical key names rather than layout-dependent keysyms.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "x_keyboard.h"
#include "../input_state.h"
#include "../menu_shortcut.h"
#include <native/app_wnd.h>
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <array>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

namespace
{
    struct x_key_state
    {
        std::array<native::key_code, 256> keys{};
        bool initialized = false;
    };
    struct named_key { const char *name; native::key_code code; };
    constexpr named_key physical_keys[] = {
        {"AD01", native::key_code::q},
        {"AD02", native::key_code::w},
        {"AD03", native::key_code::e},
        {"AD04", native::key_code::r},
        {"AD05", native::key_code::t},
        {"AD06", native::key_code::y},
        {"AD07", native::key_code::u},
        {"AD08", native::key_code::i},
        {"AD09", native::key_code::o},
        {"AD10", native::key_code::p},
        {"AC01", native::key_code::a},
        {"AC02", native::key_code::s},
        {"AC03", native::key_code::d},
        {"AC04", native::key_code::f},
        {"AC05", native::key_code::g},
        {"AC06", native::key_code::h},
        {"AC07", native::key_code::j},
        {"AC08", native::key_code::k},
        {"AC09", native::key_code::l},
        {"AB01", native::key_code::z},
        {"AB02", native::key_code::x},
        {"AB03", native::key_code::c},
        {"AB04", native::key_code::v},
        {"AB05", native::key_code::b},
        {"AB06", native::key_code::n},
        {"AB07", native::key_code::m},
        {"AE01", native::key_code::digit_1},
        {"AE02", native::key_code::digit_2},
        {"AE03", native::key_code::digit_3},
        {"AE04", native::key_code::digit_4},
        {"AE05", native::key_code::digit_5},
        {"AE06", native::key_code::digit_6},
        {"AE07", native::key_code::digit_7},
        {"AE08", native::key_code::digit_8},
        {"AE09", native::key_code::digit_9},
        {"AE10", native::key_code::digit_0},
        {"FK01", native::key_code::f1},
        {"FK02", native::key_code::f2},
        {"FK03", native::key_code::f3},
        {"FK04", native::key_code::f4},
        {"FK05", native::key_code::f5},
        {"FK06", native::key_code::f6},
        {"FK07", native::key_code::f7},
        {"FK08", native::key_code::f8},
        {"FK09", native::key_code::f9},
        {"FK10", native::key_code::f10},
        {"FK11", native::key_code::f11},
        {"FK12", native::key_code::f12},
        {"LFSH", native::key_code::left_shift},
        {"RTSH", native::key_code::right_shift},
        {"LCTL", native::key_code::left_ctrl},
        {"RCTL", native::key_code::right_ctrl},
        {"LALT", native::key_code::left_alt},
        {"RALT", native::key_code::right_alt},
        {"LWIN", native::key_code::left_meta},
        {"RWIN", native::key_code::right_meta},
        {"RTRN", native::key_code::enter},
        {"KPEN", native::key_code::keypad_enter},
        {"SPCE", native::key_code::space},
        {"BKSP", native::key_code::backspace},
        {"ESC", native::key_code::escape},
        {"TAB", native::key_code::tab},
        {"UP", native::key_code::up},
        {"DOWN", native::key_code::down},
        {"LEFT", native::key_code::left},
        {"RGHT", native::key_code::right},
        {"HOME", native::key_code::home},
        {"END", native::key_code::end},
        {"PGUP", native::key_code::page_up},
        {"PGDN", native::key_code::page_down},
        {"INS", native::key_code::insert},
        {"DELE", native::key_code::delete_key},
        {"AE11", native::key_code::minus},
        {"AE12", native::key_code::equals},
        {"AB08", native::key_code::comma},
        {"AB09", native::key_code::period},
        {"AB10", native::key_code::slash},
        {"AC10", native::key_code::semicolon},
        {"AC11", native::key_code::apostrophe},
        {"AD11", native::key_code::left_bracket},
        {"AD12", native::key_code::right_bracket},
        {"BKSL", native::key_code::backslash},
        {"TLDE", native::key_code::grave},
        {"CAPS", native::key_code::caps_lock},
        {"NMLK", native::key_code::num_lock},
        {"SCLK", native::key_code::scroll_lock},
        {"PRSC", native::key_code::print_screen},
        {"PAUS", native::key_code::pause},
        {"KPAD", native::key_code::keypad_add},
        {"KPSU", native::key_code::keypad_subtract},
        {"KPMU", native::key_code::keypad_multiply},
        {"KPDV", native::key_code::keypad_divide},
        {"KPDL", native::key_code::keypad_decimal},
        {"KP0", native::key_code::keypad_0},
        {"KP1", native::key_code::keypad_1},
        {"KP2", native::key_code::keypad_2},
        {"KP3", native::key_code::keypad_3},
        {"KP4", native::key_code::keypad_4},
        {"KP5", native::key_code::keypad_5},
        {"KP6", native::key_code::keypad_6},
        {"KP7", native::key_code::keypad_7},
        {"KP8", native::key_code::keypad_8},
        {"KP9", native::key_code::keypad_9},
    };
}
namespace native::detail
{
    bool dispatch_x_keyboard(wnd &owner, XEvent &event) {
        if (event.type == FocusIn) { owner.on_native_focus(true); return false; }
        if (event.type == FocusOut) { owner.on_native_focus(false); return false; }
        if (event.type == ButtonPress) {
            reset_input(owner);
            XSetInputFocus(event.xbutton.display, event.xbutton.window,
                RevertToParent, CurrentTime);
            owner.on_native_focus(true);
            return false;
        }
        if (event.type != KeyPress && event.type != KeyRelease) return false;
        if (!owner.get_input_enabled()) { reset_input(owner); return true; }
        auto *state = peer_state<x_key_state>(owner);
        if (!state) { state = new x_key_state; assign_peer_state(owner, state); }
        if (!state->initialized) {
            Bool supported = False;
            XkbSetDetectableAutoRepeat(event.xkey.display, True, &supported);
            XkbDescPtr keyboard = XkbGetKeyboard(event.xkey.display,
                XkbNamesMask, XkbUseCoreKbd);
            if (keyboard && keyboard->names && keyboard->names->keys) {
                for (int i = keyboard->min_key_code; i <= keyboard->max_key_code; ++i)
                    for (const auto &entry : physical_keys)
                        if (std::strncmp(keyboard->names->keys[i].name, entry.name, 4) == 0)
                            state->keys[i] = entry.code;
            }
            if (keyboard) XkbFreeKeyboard(keyboard, XkbAllComponentsMask, True);
            state->initialized = true;
        }
        // Old servers emit a release/press pair for repeat; suppress that release.
        if (event.type == KeyRelease && XPending(event.xkey.display)) {
            XEvent next;
            XPeekEvent(event.xkey.display, &next);
            if (next.type == KeyPress && next.xkey.keycode == event.xkey.keycode &&
                next.xkey.time == event.xkey.time) return true;
        }
        const key_code code = event.xkey.keycode < state->keys.size() ?
            state->keys[event.xkey.keycode] : key_code::unknown;
        unsigned mods = 0;
        if (event.xkey.state & ShiftMask) mods |= unsigned(key_modifiers::shift);
        if (event.xkey.state & ControlMask) mods |= unsigned(key_modifiers::ctrl);
        if (event.xkey.state & Mod1Mask) mods |= unsigned(key_modifiers::alt);
        if (event.xkey.state & Mod4Mask) mods |= unsigned(key_modifiers::meta);
        if (event.xkey.state & LockMask) mods |= unsigned(key_modifiers::caps_lock);
        if (event.xkey.state & XkbKeysymToModifiers(event.xkey.display, XK_Num_Lock))
            mods |= unsigned(key_modifiers::num_lock);
        const bool press = event.type == KeyPress;
        // X state precedes the event. Account for the changing modifier itself.
        unsigned bit = 0;
        if (code == key_code::left_shift || code == key_code::right_shift) bit = 1;
        if (code == key_code::left_ctrl || code == key_code::right_ctrl) bit = 2;
        if (code == key_code::left_alt || code == key_code::right_alt) bit = 4;
        if (code == key_code::left_meta || code == key_code::right_meta) bit = 8;
        if (bit) {
            const auto other = code == key_code::left_shift ? key_code::right_shift :
                code == key_code::right_shift ? key_code::left_shift :
                code == key_code::left_ctrl ? key_code::right_ctrl :
                code == key_code::right_ctrl ? key_code::left_ctrl :
                code == key_code::left_alt ? key_code::right_alt :
                code == key_code::right_alt ? key_code::left_alt :
                code == key_code::left_meta ? key_code::right_meta : key_code::left_meta;
            if (press || key_held(owner, other)) mods |= bit;
            else mods &= ~bit;
        }
        // Menu accelerators retain their toolkit handler; only skip physical
        // delivery here. Previously delivered repeats/releases remain paired.
        if (press && !key_held(owner, code)) {
            if (auto *window = dynamic_cast<app_wnd *>(&owner)) {
                const char *name = XKeysymToString(XLookupKeysym(&event.xkey, 0));
                std::string key = name ? name : "";
                std::transform(key.begin(), key.end(), key.begin(),
                    [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                for (const auto &top : window->menu.tops()) {
                    for (const auto &item : top.items) {
                        if (item.separator || item.shortcut.empty()) continue;
                        auto shortcut = parse_menu_shortcut(item.shortcut);
                        std::transform(shortcut.key.begin(), shortcut.key.end(), shortcut.key.begin(),
                            [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
                        if (shortcut.key == key && shortcut.shift == bool(mods & 1) &&
                            shortcut.control == bool(mods & 2) && shortcut.alt == bool(mods & 4) &&
                            shortcut.command == bool(mods & 8)) return false;
                    }
                }
            }
        }
        return owner.on_native_key({code, press ? key_action::press : key_action::release,
            press && key_held(owner, code), static_cast<key_modifiers>(mods)});
    }
}
