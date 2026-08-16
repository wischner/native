//
// Implements the GEMix theme by emulating the classic AES/VDI control
// look. The metrics come from the active VDI workstation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <memory>

#include <native.h>
#include <native/theme.h>

#include "../emulated_theme.h"
#include "globals.h"

namespace
{
    class gem_theme final : public linux::emulated_theme
    {
    public:
        explicit gem_theme(native::gpx &g)
            : emulated_theme(g) {}

        metrics defaults() const override {
            metrics m;
            const int character_height = std::max(
                1, static_cast<int>(linux::gemix::runtime.char_h));
            const int character_width = std::max(
                1, static_cast<int>(linux::gemix::runtime.char_w));
            m.menu_bar_height = character_height + 4;
            m.menu_item_height = character_height + 2;
            m.popup_width = character_width * 22;
            m.text_padding_x = character_width;
            return m;
        }

        palette native_palette() const override {
            palette p;
            p.button_bg = native::rgba(255, 255, 255, 255);
            p.button_border = native::rgba(0, 0, 0, 255);
            p.button_highlight = native::rgba(255, 255, 255, 255);
            p.button_shadow = native::rgba(96, 96, 96, 255);
            p.button_text = native::rgba(0, 0, 0, 255);
            p.button_disabled_text = native::rgba(128, 128, 128, 255);
            p.button_hot_bg = native::rgba(220, 220, 220, 255);
            p.button_hot_text = p.button_text;
            p.button_pressed_bg = native::rgba(0, 0, 0, 255);
            p.button_pressed_text = native::rgba(255, 255, 255, 255);
            p.menu_bar_bg = native::rgba(255, 255, 255, 255);
            p.menu_bar_line_top = native::rgba(0, 0, 0, 255);
            p.menu_bar_line_bottom = native::rgba(0, 0, 0, 255);
            p.menu_text = native::rgba(0, 0, 0, 255);
            p.menu_disabled_text = p.button_disabled_text;
            p.menu_hot_bg = native::rgba(0, 0, 0, 255);
            p.menu_hot_text = native::rgba(255, 255, 255, 255);
            p.menu_popup_bg = native::rgba(255, 255, 255, 255);
            p.menu_popup_border = native::rgba(0, 0, 0, 255);
            return p;
        }

    protected:
        int text_width(const std::string &text) const override {
            return static_cast<int>(text.size()) *
                   std::max(
                       1,
                       static_cast<int>(linux::gemix::runtime.char_w));
        }

        int text_height() const override {
            return std::max(
                1, static_cast<int>(linux::gemix::runtime.char_h / 2));
        }

        bool text_uses_baseline() const override {
            return true;
        }
    };
} // namespace

namespace native
{
    std::unique_ptr<theme> theme::create(gpx &painter) {
        return std::make_unique<gem_theme>(painter);
    }
} // namespace native
