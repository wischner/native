//
// Declares the internal control-paint backend contract.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>

#include <native.h>

namespace native
{
namespace detail
{
    // Return control dimensions selected by the active backend.
    control_paint::metrics control_paint_backend_metrics();

    // Return fallback control colors selected by the active backend.
    control_paint::palette control_paint_backend_palette();

    // Estimate the pixel width of text in the backend control font.
    int control_paint_backend_text_width(const std::string &text);

    // Return a baseline that vertically centers control text.
    int control_paint_backend_text_y_centered(const rect &r);

    // Draw a native button face, returning false when unsupported.
    bool control_paint_backend_draw_button_face_native(
        gpx &g,
        const rect &r,
        const control_paint::state &s);
    // Draw a native button frame, returning false when unsupported.
    bool control_paint_backend_draw_button_frame_native(
        gpx &g,
        const rect &r,
        const control_paint::state &s);
    // Draw native button text, returning false when unsupported.
    bool control_paint_backend_draw_button_text_native(
        gpx &g,
        const rect &r,
        const std::string &text,
        const control_paint::state &s);
    // Draw a native menu bar, returning false when unsupported.
    bool control_paint_backend_draw_menu_bar_background_native(
        gpx &g,
        const rect &r);
    // Draw a native menu-item background when supported.
    bool control_paint_backend_draw_menu_item_background_native(
        gpx &g,
        const rect &r,
        const control_paint::state &s);
    // Draw native menu-item text when supported.
    bool control_paint_backend_draw_menu_item_text_native(
        gpx &g,
        const rect &r,
        const std::string &text,
        const control_paint::state &s);
}
}
