//
// Implements the OpenMotif theme with Motif resource colors and Xme drawing
// primitives. Image targets fall back to a Motif-specific emulation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <memory>

#include <X11/StringDefs.h>
#include <Xm/DrawP.h>
#include <Xm/Xm.h>

#include <native.h>

#include "../../gpx_wnd.h"
#include "../emulated_theme.h"
#include "globals.h"

namespace
{
    struct motif_target
    {
        Widget widget = nullptr;
        linux::openmotif::motif_gpx *cache = nullptr;
    };

    motif_target target_from(native::gpx &g) {
        auto *window_gpx = dynamic_cast<native::gpx_wnd *>(&g);
        if (!window_gpx)
            return {};
        native::wnd *window = window_gpx->window();
        return {
            linux::openmotif::wnd_bindings.handle_from_object(window),
            linux::openmotif::wnd_gpx_bindings.object_from_handle(window)};
    }

    native::rgba pixel_color(Widget widget, Pixel pixel) {
        if (!widget || !linux::openmotif::cached_display)
            return native::rgba(0, 0, 0, 255);
        Colormap colormap = DefaultColormapOfScreen(XtScreen(widget));
        XtVaGetValues(widget, XtNcolormap, &colormap, nullptr);
        XColor color{};
        color.pixel = pixel;
        XQueryColor(linux::openmotif::cached_display, colormap, &color);
        return native::rgba(
            static_cast<std::uint8_t>(color.red >> 8),
            static_cast<std::uint8_t>(color.green >> 8),
            static_cast<std::uint8_t>(color.blue >> 8),
            255);
    }

    XFontStruct *query_control_font() {
        const auto &font = native::font_t::stock(native::font_role::control);
        auto *binding = linux::openmotif::font_bindings.object_from_handle(font.id());
        if (!binding || !binding->xfont || !linux::openmotif::cached_display)
            return nullptr;
        return XQueryFont(linux::openmotif::cached_display, binding->xfont);
    }

    class motif_theme final : public linux::emulated_theme
    {
    public:
        explicit motif_theme(native::gpx &g) : emulated_theme(g) {}

        metrics defaults() const override {
            metrics m;
            m.menu_bar_height = 24;
            m.menu_item_height = 20;
            m.popup_width = 180;
            m.text_padding_x = 8;
            return m;
        }

        palette native_palette() const override {
            palette p;
            p.button_bg = native::rgba(212, 208, 200, 255);
            p.button_border = native::rgba(64, 64, 64, 255);
            p.button_highlight = native::rgba(255, 255, 255, 255);
            p.button_shadow = native::rgba(96, 96, 96, 255);
            p.button_text = native::rgba(0, 0, 0, 255);
            p.button_disabled_text = native::rgba(128, 128, 128, 255);
            p.button_hot_bg = native::rgba(224, 224, 224, 255);
            p.button_hot_text = p.button_text;
            p.button_pressed_bg = native::rgba(176, 176, 176, 255);
            p.button_pressed_text = p.button_text;
            p.menu_bar_bg = p.button_bg;
            p.menu_bar_line_top = p.button_highlight;
            p.menu_bar_line_bottom = p.button_shadow;
            p.menu_text = p.button_text;
            p.menu_disabled_text = p.button_disabled_text;
            p.menu_hot_bg = native::rgba(0, 0, 128, 255);
            p.menu_hot_text = native::rgba(255, 255, 255, 255);
            p.menu_popup_bg = p.button_bg;
            p.menu_popup_border = p.button_border;

            motif_target target = target_from(_g);
            if (!target.widget)
                return p;

            Pixel background = 0;
            Pixel foreground = 0;
            Pixel top_shadow = 0;
            Pixel bottom_shadow = 0;
            Pixel highlight = 0;
            XtVaGetValues(
                target.widget,
                XmNbackground, &background,
                XmNforeground, &foreground,
                XmNtopShadowColor, &top_shadow,
                XmNbottomShadowColor, &bottom_shadow,
                XmNhighlightColor, &highlight,
                nullptr);
            p.button_bg = pixel_color(target.widget, background);
            p.button_text = pixel_color(target.widget, foreground);
            p.button_highlight = pixel_color(target.widget, top_shadow);
            p.button_shadow = pixel_color(target.widget, bottom_shadow);
            p.button_border = p.button_shadow;
            p.button_hot_bg = pixel_color(target.widget, highlight);
            p.button_hot_text = p.button_text;
            p.button_pressed_bg = p.button_shadow;
            p.menu_bar_bg = p.button_bg;
            p.menu_bar_line_top = p.button_highlight;
            p.menu_bar_line_bottom = p.button_shadow;
            p.menu_text = p.button_text;
            p.menu_popup_bg = p.button_bg;
            p.menu_popup_border = p.button_shadow;
            return p;
        }

        theme &draw_button(
            const native::rect &r,
            const std::string &text,
            const state &s) override {
            emulated_theme::draw_button(r, text, s);
            draw_shadow(r, s.pressed ? XmSHADOW_IN : XmSHADOW_OUT, 2);
            return *this;
        }

        theme &draw_menu_bar(const native::rect &r) override {
            emulated_theme::draw_menu_bar(r);
            draw_shadow(r, XmSHADOW_OUT, 1);
            return *this;
        }

        theme &draw_popup_frame(const native::rect &r) override {
            emulated_theme::draw_popup_frame(r);
            draw_shadow(r, XmSHADOW_OUT, 2);
            return *this;
        }

    protected:
        int text_width(const std::string &text) const override {
            XFontStruct *font = query_control_font();
            if (!font)
                return static_cast<int>(text.size()) * 7;
            const int width = XTextWidth(
                font,
                text.c_str(),
                static_cast<int>(text.size()));
            XFreeFontInfo(nullptr, font, 1);
            return width;
        }

        int text_height() const override {
            XFontStruct *font = query_control_font();
            if (!font)
                return 8;
            const int height = std::max(1, font->ascent - font->descent);
            XFreeFontInfo(nullptr, font, 1);
            return height;
        }

        bool text_uses_baseline() const override {
            return true;
        }

    private:
        void draw_shadow(
            const native::rect &r,
            unsigned int shadow_type,
            unsigned int thickness) {
            motif_target target = target_from(_g);
            if (!target.widget || !target.cache || !target.cache->backbuffer ||
                !linux::openmotif::cached_display || !r.d.w || !r.d.h)
                return;

            Pixel top = 0;
            Pixel bottom = 0;
            XtVaGetValues(
                target.widget,
                XmNtopShadowColor, &top,
                XmNbottomShadowColor, &bottom,
                nullptr);
            XGCValues values{};
            values.foreground = top;
            GC top_gc = XCreateGC(
                linux::openmotif::cached_display,
                target.cache->backbuffer,
                GCForeground,
                &values);
            values.foreground = bottom;
            GC bottom_gc = XCreateGC(
                linux::openmotif::cached_display,
                target.cache->backbuffer,
                GCForeground,
                &values);
            XmeDrawShadows(
                linux::openmotif::cached_display,
                target.cache->backbuffer,
                top_gc,
                bottom_gc,
                r.p.x,
                r.p.y,
                r.d.w,
                r.d.h,
                thickness,
                shadow_type);
            XFreeGC(linux::openmotif::cached_display, top_gc);
            XFreeGC(linux::openmotif::cached_display, bottom_gc);
        }
    };
}

namespace native
{
    std::unique_ptr<theme> theme::create(gpx &painter) {
        return std::make_unique<motif_theme>(painter);
    }
}
