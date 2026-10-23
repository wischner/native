//
// Keeps the existing property canvas on adapters without private native
// combo-popup composition hooks.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "combo_picker.h"
#include <stdexcept>
namespace native::detail
{
    // Adapters without this composition never create the private field.
    point picker_popup_position(combo_box &) {
        throw std::logic_error(
            "This adapter has no composed combo popup.");
    }

    bool supports_combo_picker_actions() {
        return false;
    }
} // namespace native::detail
