//
// Adapts GEM's optional physical-key packets to Native's held-key contract.
// Standard AES text handling remains the fallback for unconsumed presses.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "../../input_state.h"

#include <string>

namespace
{
    // Themed GEM controls need the keyboard commands a native widget owns.
    bool activate_control(native::wnd &target, native::key_code code,
                          WORD modifiers, bool repeat) {
        using namespace native;
        if (modifiers & 12) return false;
        if (code == key_code::space) {
            if (auto *control = dynamic_cast<check *>(&target)) {
                if (!repeat) control->on_native_checked(!control->get_checked());
                return true;
            }
            if (auto *control = dynamic_cast<radio *>(&target)) {
                if (!repeat) control->on_native_selected();
                return true;
            }
        }
        if (code == key_code::space || code == key_code::enter ||
            code == key_code::keypad_enter) {
            if (auto *control = dynamic_cast<button *>(&target)) {
                if (!repeat) control->on_native_click();
                return true;
            }
        }
        return false;
    }
}

namespace linux::gemix
{
    bool handle_canvas_key(native::app_wnd *parent, WORD modifiers, WORD key) {
        const char ascii = static_cast<char>(key & 0xff);
        if ((modifiers & 12) != 0 || ascii < 32 || ascii > 126)
            return false;
        return native::detail::deliver_canvas_text(*parent,
                                                   std::string(1, ascii));
    }

    bool dispatch_physical_key(native::app_wnd &owner, WORD modifiers,
                               WORD key, WORD state) {
#ifdef NATIVE_GEM_PHYSICAL_KEYBOARD
        using namespace native;
        if (state & GEM_KEY_RESET) {
            detail::reset_input(owner);
            return true;
        }
        auto *target = detail::input_target(owner);
        if (!target) target = &owner;
        const auto code = detail::usb_key((unsigned(key) >> 8) & 255);
        const bool pressed = (state & GEM_KEY_PRESS) != 0;
        key_modifiers mods = key_modifiers::none;
        if (modifiers & 3) mods = mods | key_modifiers::shift;
        if (modifiers & 4) mods = mods | key_modifiers::ctrl;
        if (modifiers & 8) mods = mods | key_modifiers::alt;
        const bool repeat = pressed && detail::key_held(*target, code);
        const detail::window_guard guard(*target);
        const bool consumed = target->on_native_key({code,
            pressed ? key_action::press : key_action::release,
            repeat, mods});
        if (consumed && pressed && code == key_code::tab && guard.alive())
            dismiss_combo_popups(owner);
        if (consumed || !pressed || !guard.alive()) return true;
        return activate_control(*target, code, modifiers, repeat);
#else
        auto *target = native::detail::input_target(owner);
        if (!target) target = &owner;
        const auto ascii = key & 255;
        const auto code = ascii == 9 ? native::key_code::tab :
            ascii == 13 ? native::key_code::enter :
            ascii == 32 ? native::key_code::space : native::key_code::unknown;
        unsigned mods = (modifiers & 3 ? 1 : 0) |
            (modifiers & 4 ? 2 : 0) | (modifiers & 8 ? 4 : 0);
        const native::detail::window_guard owner_guard(owner);
        if (target->on_native_tab({code, native::key_action::press, false,
                                  static_cast<native::key_modifiers>(mods)})) {
            if (owner_guard.alive()) dismiss_combo_popups(owner);
            return true;
        }
        (void)state;
        return activate_control(*target, code, modifiers, false);
#endif
    }
} // namespace linux::gemix
