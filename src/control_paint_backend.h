#pragma once

#include <string>

#include <native.h>

namespace native
{
namespace detail
{
    control_paint::metrics control_paint_backend_metrics();
    control_paint::palette control_paint_backend_palette();
    int control_paint_backend_text_width(const std::string &text);
    int control_paint_backend_text_y_centered(const rect &r);
    bool control_paint_backend_draw_button_face_native(
        gpx &g,
        const rect &r,
        const control_paint::state &s);
    bool control_paint_backend_draw_button_frame_native(
        gpx &g,
        const rect &r,
        const control_paint::state &s);
    bool control_paint_backend_draw_button_text_native(
        gpx &g,
        const rect &r,
        const std::string &text,
        const control_paint::state &s);
    bool control_paint_backend_draw_menu_bar_background_native(
        gpx &g,
        const rect &r);
    bool control_paint_backend_draw_menu_item_background_native(
        gpx &g,
        const rect &r,
        const control_paint::state &s);
    bool control_paint_backend_draw_menu_item_text_native(
        gpx &g,
        const rect &r,
        const std::string &text,
        const control_paint::state &s);
}
}
