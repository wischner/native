//
// Implements the X11/Athena theme. Xaw does not expose a painter for
// arbitrary drawables, so custom targets emulate Athena's raised bevels and
// monochrome selection behavior inside this backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <memory>

#include <X11/Xlib.h>

#include <native.h>

#include "../emulated_theme.h"
#include "globals.h"

namespace
{
    XFontStruct *query_control_font() {
        const auto &font = native::font_t::stock(native::font_role::control);
        auto *binding = linux::x11::font_bindings.object_from_handle(font.id());
        if (!binding || !binding->xfont || !linux::x11::cached_display)
            return nullptr;
        return XQueryFont(linux::x11::cached_display, binding->xfont);
    }

    class xaw_theme final : public linux::emulated_theme
    {
    public:
        explicit xaw_theme(native::gpx &g) : emulated_theme(g) {}

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
            p.button_bg = native::rgba(214, 214, 214, 255);
            p.button_border = native::rgba(70, 70, 70, 255);
            p.button_highlight = native::rgba(255, 255, 255, 255);
            p.button_shadow = native::rgba(118, 118, 118, 255);
            p.button_text = native::rgba(0, 0, 0, 255);
            p.button_disabled_text = native::rgba(128, 128, 128, 255);
            p.button_hot_bg = native::rgba(228, 228, 228, 255);
            p.button_hot_text = p.button_text;
            p.button_pressed_bg = native::rgba(190, 190, 190, 255);
            p.button_pressed_text = p.button_text;
            p.menu_bar_bg = p.button_bg;
            p.menu_bar_line_top = p.button_highlight;
            p.menu_bar_line_bottom = p.button_shadow;
            p.menu_text = p.button_text;
            p.menu_disabled_text = p.button_disabled_text;
            p.menu_hot_bg = native::rgba(0, 0, 0, 255);
            p.menu_hot_text = native::rgba(255, 255, 255, 255);
            p.menu_popup_bg = p.button_bg;
            p.menu_popup_border = p.button_border;
            return p;
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
                return 12;
            const int height = std::max(1, font->ascent - font->descent);
            XFreeFontInfo(nullptr, font, 1);
            return height;
        }

        bool text_uses_baseline() const override {
            return true;
        }
    };
}

namespace native
{
    std::unique_ptr<theme> theme::create(gpx &painter) {
        return std::make_unique<xaw_theme>(painter);
    }
}
