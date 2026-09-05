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
#include "stock_text.h"

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
            m.header_height = character_height + 6;
            // AES titles and menu chrome reserve six pixels around the font.
            m.status_bar_height = character_height + 6;
            m.tab_height = m.header_height;
            m.disclosure_size = std::max(5, character_height - 2);
            m.icon_view_padding_x = std::max(2, character_width);
            m.icon_view_padding_y = 4;
            return m;
        }

        palette native_palette() const override {
            palette p;
            p.button_bg = native::rgba(255, 255, 255, 255);
            p.button_border = native::rgba(0, 0, 0, 255);
            p.button_highlight = native::rgba(0, 0, 0, 255);
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
            p.content_bg = p.menu_popup_bg;
            p.content_text = p.button_text;
            p.selection_bg = p.menu_hot_bg;
            p.selection_text = p.menu_hot_text;
            p.selection_inactive_bg = p.button_shadow;
            p.selection_inactive_text = p.button_text;
            p.separator = p.button_border;
            p.focus = p.button_border;
            return p;
        }

        theme &draw_radio(const native::rect &bounds,
                          const std::string &text, const state &s) override {
            auto saved = _g.save_state();
            const auto colors = native_palette();
            const int extent = std::max(5, std::min(14, int(bounds.h()) - 4));
            const native::rect indicator(bounds.x1() + 2,
                bounds.y1() + (bounds.h() - extent) / 2, extent, extent);
            _g.set_ink(colors.button_bg).draw_ellipse(indicator, true)
                .set_ink(colors.button_border).draw_ellipse(indicator, false);
            if (s.selected) {
                const int inset = std::max(2, extent / 3);
                _g.draw_ellipse(native::rect(indicator.x1() + inset,
                    indicator.y1() + inset, extent - 2 * inset, extent - 2 * inset), true);
            }
            _g.set_font(native::font_t::stock(native::font_role::control))
                .set_ink(colors.button_text).draw_text(text,
                    native::rect(bounds.x1() + extent + 8, bounds.y1(),
                        std::max(0, int(bounds.w()) - extent - 8), bounds.h()),
                    {native::text_align::start, native::text_valign::center,
                     native::text_overflow::ellipsis, true});
            return *this;
        }

        theme &draw_surface(const native::rect &r, native::surface_kind kind,
                            const state &s) override {
            if (kind == native::surface_kind::ruler)
                kind = native::surface_kind::header;
            if (kind == native::surface_kind::status_part) {
                if (!r.w() || !r.h()) return *this;
                auto saved = _g.save_state();
                const auto colors = native_palette();
                // The window supplies the bottom/right enclosure. Keep
                // only the top rule and each part's leading divider.
                _g.set_pen(1).set_ink(colors.button_bg).draw_rect(r, true)
                    .set_ink(colors.button_border)
                    .draw_line(r.p, native::point(r.x2() - 1, r.y1()))
                    .draw_line(r.p, native::point(r.x1(), r.y2() - 1));
                return *this;
            }
            draw_surface_fallback(r, kind, s);
            if (kind == native::surface_kind::header ||
                kind == native::surface_kind::table_header)
                _g.set_ink(native_palette().button_border).draw_rect(r, false);
            if (kind == native::surface_kind::status && r.w() && r.h())
                _g.set_ink(native_palette().button_border).draw_line(
                    r.p, native::point(r.x2() - 1, r.y1()));
            return *this;
        }

        theme &draw_scrollbar_part(const native::rect &r,
            native::scrollbar_orientation axis, native::scrollbar_part part,
            const state &s) override {
            if (!r.w() || !r.h()) return *this;
            auto saved = _g.save_state();
            const auto colors = native_palette();
            _g.set_pen(1).set_ink(colors.button_bg).draw_rect(r, true)
                .set_ink(colors.button_border).draw_rect(r, false);
            if (part == native::scrollbar_part::track) {
                for (int y = r.y1() + 2; y < r.y2() - 1; y += 2)
                    for (int x = r.x1() + 2 + (y & 2); x < r.x2() - 1; x += 4)
                        _g.draw_line(native::point(x, y), native::point(x, y));
            } else if (part != native::scrollbar_part::thumb) {
                const int cx = r.x1() + r.w() / 2;
                const int cy = r.y1() + r.h() / 2;
                const int direction = part == native::scrollbar_part::decrement ? -1 : 1;
                if (s.pressed) {
                    _g.draw_rect(r, true).set_ink(colors.button_bg);
                }
                for (int step = 0; step < 4; ++step) {
                    const int along = direction * (3 - step);
                    if (axis == native::scrollbar_orientation::vertical)
                        _g.draw_line(native::point(cx - step, cy + along),
                                     native::point(cx + step, cy + along));
                    else _g.draw_line(native::point(cx + along, cy - step),
                                      native::point(cx + along, cy + step));
                }
            }
            return *this;
        }

    protected:
        int text_width(const std::string &text) const override {
            return static_cast<int>(linux::gemix::stock_text(text).size()) *
                   std::max(
                       1,
                       static_cast<int>(linux::gemix::runtime.char_w));
        }

        int text_height() const override {
            return std::max(
                1, static_cast<int>(linux::gemix::runtime.char_h));
        }
    };
} // namespace

namespace native
{
    std::unique_ptr<theme> theme::create(gpx &painter) {
        return std::make_unique<gem_theme>(painter);
    }
} // namespace native
