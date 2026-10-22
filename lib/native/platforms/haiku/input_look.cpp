//
// Keeps selection, editable and date dropdown indicators on one native
// BControlLook drawing path, including their padding, color and separator.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "input_look.h"
#include <ControlLook.h>
#include <algorithm>
#include <cmath>

namespace haiku
{
    BRect draw_combo_background(BControl &control, BRect bounds, const BRect &update) {
        const auto field = bounds;
        const auto base = ui_color(B_PANEL_BACKGROUND_COLOR);
        const uint32 flags = be_control_look->Flags(&control);
        be_control_look->DrawMenuFieldFrame(&control, bounds, update, base, base, flags);
        be_control_look->DrawMenuFieldBackground(&control, bounds, update, base, false, flags);
        draw_combo_indicator(control, combo_indicator_bounds(field), update);
        return bounds;
    }

    BRect combo_indicator_bounds(BRect field) {
        field.InsetBy(2, 2);
        field.left = std::max(field.left, field.right - 13);
        return field;
    }

    void draw_combo_indicator(BControl &control, BRect bounds, const BRect &update) {
        const auto base = ui_color(B_PANEL_BACKGROUND_COLOR);
        const uint32 flags = be_control_look->Flags(&control);
        if (flags & BControlLook::B_ACTIVATED) {
            auto pressed = bounds;
            be_control_look->DrawButtonBackground(&control, pressed, update, base, flags);
        }
        const float left = std::floor((bounds.left + bounds.right) / 2) - 6;
        const float top = std::floor((bounds.top + bounds.bottom) / 2) - 6;
        BRect arrow(left, top, left + 12, top + 12);
        be_control_look->DrawArrowShape(&control, arrow, update,
            base, BControlLook::B_DOWN_ARROW, flags);
    }
}
