//
// Adapts GEM's optional physical-key packets to Native's held-key contract.
// Standard AES text handling remains the fallback for unconsumed presses.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "../../input_state.h"

namespace linux::gemix
{
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
        const bool consumed = target->on_native_key({code,
            pressed ? key_action::press : key_action::release,
            pressed && detail::key_held(*target, code), mods});
        return consumed || !pressed;
#else
        (void)owner;
        (void)modifiers;
        (void)key;
        (void)state;
        return false;
#endif
    }
} // namespace linux::gemix
