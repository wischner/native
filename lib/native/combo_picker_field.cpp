//
// Publishes private combo composition callbacks after peer allocation
// and before native widget creation, preserving configuration across
// retries.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "combo_picker.h"
#include "wnd_peer.h"

namespace native::detail
{
    combo_picker_field::combo_picker_field()
        : combo_box({}, combo_box_style::editable) {}

    combo_picker_actions &combo_picker_field::configured_actions() {
        return _actions;
    }

    void combo_picker_field::create_native() {
        assign_peer_state(*this, new combo_picker_actions(_actions));
        combo_box::create_native();
    }
} // namespace native::detail
