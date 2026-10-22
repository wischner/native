//
// Shares Haiku's native menu-field indicator between combo and date inputs.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <Control.h>
namespace haiku
{
    // Draw the native menu-field background; the view's clip can expose only
    // its indicator when this is an accessory inside another native field.
    BRect draw_combo_background(BControl &control, BRect bounds, const BRect &update);
    // Reserve a narrow, inset native dropdown button inside a field.
    BRect combo_indicator_bounds(BRect field);
    // Center the native arrow in its button and draw native pressed feedback.
    void draw_combo_indicator(BControl &control, BRect bounds, const BRect &update);
}
