//
// Owns peer-local physical input state and portable receiver lifetime tokens.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once
#include <bitset>
#include <atomic>
#include <native/events.h>
#include <native/wnd.h>
#include "wnd_peer.h"

namespace native::detail
{
    struct wnd_lifetime { std::atomic<bool> alive{true}; };
    struct input_state
    {
        std::bitset<static_cast<unsigned>(key_code::count)> held;
        bool focused = false;
        key_modifiers modifiers = key_modifiers::none;
    };

    // Find focused descendants without a process-wide window registry.
    wnd *input_target(wnd &root);
    // Cancel a complete owner input tree before a panel or menu takes input.
    void reset_input(wnd &root);
    // Test held ownership before accelerator dispatch.
    bool key_held(wnd &target, key_code key);
    // Private numeric physical mappings; values never enter the public enum.
    key_code usb_key(unsigned position);
    key_code pc_key(unsigned scan, bool extended);
}
