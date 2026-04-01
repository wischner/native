#include <string>

#include <native.h>

#include "../../control_paint_backend.h"

namespace native
{
namespace detail
{
    control_paint::metrics control_paint_backend_metrics()
    {
        control_paint::metrics m;
        m.menu_bar_height = 20;
        m.menu_item_height = 20;
        m.popup_width = 180;
        m.text_padding_x = 8;
        return m;
    }

    control_paint::palette control_paint_backend_palette()
    {
        control_paint::palette p;
        p.button_bg = rgba(212, 208, 200, 255);
        p.button_border = rgba(0, 0, 0, 255);
        p.button_text = rgba(0, 0, 0, 255);
        p.button_hot_bg = rgba(200, 200, 200, 255);
        p.button_hot_text = rgba(0, 0, 0, 255);
        p.button_pressed_bg = rgba(176, 176, 176, 255);
        p.button_pressed_text = rgba(0, 0, 0, 255);

        p.menu_bar_bg = rgba(212, 208, 200, 255);
        p.menu_bar_line_top = rgba(255, 255, 255, 255);
        p.menu_bar_line_bottom = rgba(0, 0, 0, 255);
        p.menu_text = rgba(0, 0, 0, 255);
        p.menu_hot_bg = rgba(0, 0, 128, 255);
        p.menu_hot_text = rgba(255, 255, 255, 255);
        p.menu_popup_bg = rgba(212, 208, 200, 255);
        p.menu_popup_border = rgba(0, 0, 0, 255);
        return p;
    }

    int control_paint_backend_text_width(const std::string &text)
    {
        return static_cast<int>(text.size()) * 7;
    }

    int control_paint_backend_text_y_centered(const rect &r)
    {
        return r.p.y + static_cast<int>(r.d.h) / 2 + 4;
    }

    bool control_paint_backend_draw_button_face_native(
        gpx &,
        const rect &,
        const control_paint::state &)
    {
        return false;
    }

    bool control_paint_backend_draw_button_frame_native(
        gpx &,
        const rect &,
        const control_paint::state &)
    {
        return false;
    }

    bool control_paint_backend_draw_button_text_native(
        gpx &,
        const rect &,
        const std::string &,
        const control_paint::state &)
    {
        return false;
    }

    bool control_paint_backend_draw_menu_bar_background_native(
        gpx &,
        const rect &)
    {
        return false;
    }

    bool control_paint_backend_draw_menu_item_background_native(
        gpx &,
        const rect &,
        const control_paint::state &)
    {
        return false;
    }

    bool control_paint_backend_draw_menu_item_text_native(
        gpx &,
        const rect &,
        const std::string &,
        const control_paint::state &)
    {
        return false;
    }
}
}
