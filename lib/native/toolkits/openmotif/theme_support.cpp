//
// Implements private OpenMotif theme resource and drawing helpers.
// The helpers keep Xt and Xlib details out of the semantic painter.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "theme_support.h"

#include <X11/StringDefs.h>
#include <Xm/PushB.h>

#include "../../gpx_wnd.h"
#include "globals.h"

namespace linux::openmotif
{
    theme_target theme_target_from(native::gpx &graphics) {
        auto *window_graphics =
            dynamic_cast<native::gpx_wnd *>(&graphics);
        if (!window_graphics)
            return {};
        native::wnd *window = window_graphics->window();
        return {window,
                wnd_bindings.handle_from_object(window),
                wnd_gpx_bindings.object_from_handle(window)};
    }

    Widget theme_reference_widget(native::gpx &graphics) {
        const theme_target target = theme_target_from(graphics);
        if (target.widget)
            return target.widget;
        native::app_wnd *main_window = native::app::main_wnd();
        return main_window
                   ? wnd_bindings.handle_from_object(main_window)
                   : nullptr;
    }

    native::rgba theme_pixel_color(Widget widget, Pixel pixel) {
        if (!widget || !cached_display)
            return native::rgba(0, 0, 0, 255);
        Colormap colormap = DefaultColormapOfScreen(XtScreen(widget));
        XtVaGetValues(widget, XtNcolormap, &colormap, nullptr);
        XColor color{};
        color.pixel = pixel;
        XQueryColor(cached_display, colormap, &color);
        return native::rgba(
            static_cast<std::uint8_t>(color.red >> 8),
            static_cast<std::uint8_t>(color.green >> 8),
            static_cast<std::uint8_t>(color.blue >> 8),
            255);
    }

    native::theme::palette theme_palette(Widget reference,
                                         Widget button,
                                         Widget list) {
        native::theme::palette result;
        result.button_bg = native::rgba(192, 192, 192, 255);
        result.button_border = native::rgba(96, 96, 96, 255);
        result.button_highlight = native::rgba(255, 255, 255, 255);
        result.button_shadow = native::rgba(96, 96, 96, 255);
        result.button_text = native::rgba(0, 0, 0, 255);
        result.button_disabled_text = result.button_shadow;
        result.button_hot_bg = result.button_bg;
        result.button_hot_text = result.button_text;
        result.button_pressed_bg = native::rgba(160, 160, 160, 255);
        result.button_pressed_text = result.button_text;
        result.menu_bar_bg = result.button_bg;
        result.menu_bar_line_top = result.button_highlight;
        result.menu_bar_line_bottom = result.button_shadow;
        result.menu_text = result.button_text;
        result.menu_disabled_text = result.button_disabled_text;
        result.menu_hot_bg = result.button_text;
        result.menu_hot_text = result.button_bg;
        result.menu_popup_bg = result.button_bg;
        result.menu_popup_border = result.button_shadow;
        result.content_bg = native::rgba(255, 255, 255, 255);
        result.content_text = result.button_text;
        result.selection_bg = result.menu_hot_bg;
        result.selection_text = result.menu_hot_text;
        result.selection_inactive_bg = result.button_shadow;
        result.selection_inactive_text = result.content_text;
        result.separator = result.button_shadow;
        result.focus = result.button_text;
        if (!reference)
            return result;

        Pixel background = 0;
        Pixel foreground = 0;
        Pixel top_shadow = 0;
        Pixel bottom_shadow = 0;
        Pixel arm = 0;
        XtVaGetValues(button ? button : reference,
                      XmNbackground,
                      &background,
                      XmNforeground,
                      &foreground,
                      XmNtopShadowColor,
                      &top_shadow,
                      XmNbottomShadowColor,
                      &bottom_shadow,
                      nullptr);
        if (button)
            XtVaGetValues(button, XmNarmColor, &arm, nullptr);
        else
            arm = bottom_shadow;
        result.button_bg = theme_pixel_color(reference, background);
        result.button_text = theme_pixel_color(reference, foreground);
        result.button_highlight =
            theme_pixel_color(reference, top_shadow);
        result.button_shadow =
            theme_pixel_color(reference, bottom_shadow);
        result.button_border = result.button_shadow;
        result.button_disabled_text = result.button_shadow;
        result.button_hot_bg = result.button_bg;
        result.button_hot_text = result.button_text;
        result.button_pressed_bg = theme_pixel_color(reference, arm);
        result.button_pressed_text = result.button_text;
        result.menu_bar_bg = result.button_bg;
        result.menu_bar_line_top = result.button_highlight;
        result.menu_bar_line_bottom = result.button_shadow;
        result.menu_text = result.button_text;
        result.menu_disabled_text = result.button_disabled_text;
        result.menu_popup_bg = result.button_bg;
        result.menu_popup_border = result.button_shadow;

        if (list) {
            XtVaGetValues(list,
                          XmNbackground,
                          &background,
                          XmNforeground,
                          &foreground,
                          XmNbottomShadowColor,
                          &bottom_shadow,
                          nullptr);
            result.menu_popup_bg =
                theme_pixel_color(reference, background);
            result.menu_popup_border =
                theme_pixel_color(reference, bottom_shadow);
            result.menu_hot_bg =
                theme_pixel_color(reference, foreground);
            result.menu_hot_text =
                theme_pixel_color(reference, background);
        }
        result.content_bg = result.menu_popup_bg;
        result.content_text = result.button_text;
        result.selection_bg = result.menu_hot_bg;
        result.selection_text = result.menu_hot_text;
        result.selection_inactive_bg = result.button_shadow;
        result.selection_inactive_text = result.content_text;
        result.separator = result.button_shadow;
        result.focus = result.button_text;
        return result;
    }

    XFontStruct *theme_control_font() {
        const auto &font =
            native::font_t::stock(native::font_role::control);
        auto *binding = font_bindings.object_from_handle(font.id());
        if (!binding || !binding->xfont || !cached_display)
            return nullptr;
        return XQueryFont(cached_display, binding->xfont);
    }

    GC theme_gc(const theme_target &target,
                Pixel color,
                const native::rect &clip) {
        if (!cached_display || !target.cache ||
            !target.cache->backbuffer) {
            return nullptr;
        }
        XGCValues values{};
        values.foreground = color;
        GC result = XCreateGC(cached_display,
                              target.cache->backbuffer,
                              GCForeground,
                              &values);
        XRectangle rectangle = {
            static_cast<short>(clip.p.x),
            static_cast<short>(clip.p.y),
            static_cast<unsigned short>(clip.d.w),
            static_cast<unsigned short>(clip.d.h)};
        XSetClipRectangles(cached_display,
                           result,
                           0,
                           0,
                           &rectangle,
                           1,
                           Unsorted);
        return result;
    }
} // namespace linux::openmotif
