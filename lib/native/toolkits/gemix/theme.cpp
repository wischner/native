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
extern "C" {
#include <gem/gemctl.h>
}

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
            // Ask the same layout routine as Gemscape and Ted. A tall
            // work area obtains the natural band without viewport clipping.
            const GRECT work{0, 0, 1024, 1024};
            gemctl_chrome_t chrome{};
            gemctl_chrome_layout(&work, linux::gemix::runtime.char_h,
                                 linux::gemix::runtime.box_h, &chrome);
            m.toolbar_height = chrome.band_height;
            m.status_bar_height = chrome.band_height;
            m.toolbar_padding = 0;
            m.toolbar_control_padding = 1;
            // GEM groups commands with gaps as wide as one square tool.
            m.toolbar_separator_length = 0;
            m.text_edit_height = m.toolbar_height - 2;
            m.tab_height = 20;
            m.disclosure_size = std::clamp(character_height / 2, 5, 9);
            m.tree_indent_width = 9;
            m.tree_horizontal_padding = 3;
            m.tree_item_gap = 2;
            m.icon_view_padding_x = std::max(2, character_width);
            m.icon_view_padding_y = 4;
            return m;
        }

        theme &draw_menu_bar(const native::rect &r) override {
            if (!r.w() || !r.h())
                return *this;
            auto saved = _g.save_state();
            const auto colors = native_palette();
            _g.set_ink(colors.menu_bar_bg)
                .draw_rect(r, true)
                .set_ink(colors.menu_bar_line_bottom);
            if (r.h() > r.w())
                _g.draw_line(native::point(r.x2() - 1, r.y1()),
                             native::point(r.x2() - 1, r.y2() - 1));
            else
                _g.draw_line(native::point(r.x1(), r.y2() - 1),
                             native::point(r.x2() - 1, r.y2() - 1));
            return *this;
        }
        theme &draw_toolbar_border(const native::rect &r,
                                   native::window_edge edge,
                                   native::border_sides sides) override {
            native::border_sides inward = native::border_sides::none;
            switch (edge) {
            case native::window_edge::top:
                inward = native::border_sides::bottom; break;
            case native::window_edge::bottom:
                inward = native::border_sides::top; break;
            case native::window_edge::left:
                inward = native::border_sides::right; break;
            case native::window_edge::right:
                inward = native::border_sides::left; break;
            }
            auto saved = _g.save_state();
            _g.set_pen(1).set_ink(native_palette().menu_bar_line_bottom)
                .draw_border(r, inward & sides);
            return *this;
        }

        theme &draw_toolbar_button(const native::rect &r,
                                   const std::string &text,
                                   const state &element_state) override {
            if (!r.w() || !r.h()) return *this;
            auto saved = _g.save_state();
            // GEM slots share a trailing separator and the band's bottom
            // rule. Keep selection feedback inside those lines.
            draw_menu_title(native::rect(r.x1(), r.y1(),
                r.w() - 1, r.h() - 1), text, element_state);
            _g.set_ink(native_palette().menu_bar_line_bottom).set_pen(1)
                .draw_line(native::point(r.x2() - 1, r.y1()),
                           native::point(r.x2() - 1, r.y2() - 1))
                .draw_line(native::point(r.x1(), r.y2() - 1),
                           native::point(r.x2() - 1, r.y2() - 1));
            return *this;
        }

        theme &draw_toolbar_separator(
            const native::rect &r,
            native::separator_orientation orientation) override {
            if (!r.w() || !r.h()) return *this;
            auto saved = _g.save_state();
            // End the empty group slot where the next square tool begins.
            // A centered rule would visually merge half the gap into it.
            _g.set_pen(1).set_ink(native_palette().menu_bar_line_bottom)
                .draw_border(r,
                    orientation == native::separator_orientation::vertical
                        ? native::border_sides::right
                        : native::border_sides::bottom);
            return *this;
        }

        theme &draw_list(const native::rect &r,
                         const std::vector<std::string> &items,
                         int selected,
                         const state &s) override {
            auto saved = _g.save_state();
            const auto colors = native_palette();
            _g.set_ink(colors.content_bg)
                .draw_rect(r, true)
                .set_ink(colors.button_border)
                .draw_rect(r, false);
            if (r.w() <= 4 || r.h() <= 2)
                return *this;
            const native::rect inner(
                r.x1() + 2, r.y1() + 1, r.w() - 4, r.h() - 2);
            _g.set_clip(_g.get_clip().intersect(inner));
            const int height = get_list_item_height();
            for (std::size_t i = 0;
                 i < items.size() &&
                 inner.y1() + int(i) * height < inner.y2();
                 ++i) {
                auto row = s;
                row.selected = int(i) == selected;
                draw_list_item(
                    native::rect(inner.x1(),
                                 inner.y1() + int(i) * height,
                                 inner.w(),
                                 height),
                    items[i],
                    row);
            }
            return *this;
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

        theme &draw_splitter_grip(
            const native::rect &bounds,
            native::separator_orientation orientation,
            const state &) override {
            if (!bounds.w() || !bounds.h())
                return *this;
            auto saved = _g.save_state();
            const int x = bounds.x1() + bounds.w() / 2;
            const int y = bounds.y1() + bounds.h() / 2;
            _g.set_pen(1).set_ink(native_palette().separator);
            for (int offset : {-2, 0, 2}) {
                const native::point dot(
                    x + (orientation == native::separator_orientation::horizontal
                             ? offset : 0),
                    y + (orientation == native::separator_orientation::vertical
                             ? offset : 0));
                if (bounds.contains(dot))
                    _g.draw_rect(native::rect(dot, native::size(1, 1)), true);
            }
            return *this;
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
            // Table headings use the table's outer frame and column
            // separators. Other header surfaces retain their outline.
            if (kind == native::surface_kind::header)
                _g.set_ink(native_palette().button_border).draw_rect(r, false);
            if (kind == native::surface_kind::status && r.w() && r.h())
                _g.set_ink(native_palette().button_border).draw_line(
                    r.p, native::point(r.x2() - 1, r.y1()));
            return *this;
        }

        theme &draw_table_group_background(const native::rect &bounds,
                                            const state &s) override {
            auto saved = _g.save_state();
            draw_surface_fallback(bounds, native::surface_kind::header, s);
            // The preceding header or row supplies the upper boundary.
            _g.set_pen(1).set_ink(native_palette().button_border)
                .draw_border(bounds, native::border_sides::left |
                    native::border_sides::bottom | native::border_sides::right);
            return *this;
        }

        theme &draw_toolbar_icon(const native::img &image,
                                 const native::rect &bounds,
                                 const state &element_state) override {
            const auto *pixels = image.pixels();
            const auto count = std::size_t(image.w()) * image.h();
            const bool mask = std::all_of(pixels, pixels + count,
                [](native::rgba pixel) {
                    return !pixel.a || (!pixel.r && !pixel.g && !pixel.b);
                });
            if (!element_state.disabled || !mask)
                return theme::draw_toolbar_icon(image, bounds, element_state);
            // Gray image ink thresholds to white in GEM. Match disabled
            // stock text with a binary stipple instead of losing the icon.
            auto saved = _g.save_state();
            native::img stippled(image.w(), image.h());
            const auto ink = native_palette().menu_text;
            for (int y = 0; y < image.h(); ++y)
                for (int x = 0; x < image.w(); ++x) {
                    const auto index = y * image.w() + x;
                    stippled.pixels()[index] = native::rgba(
                        ink.r, ink.g, ink.b,
                        (x + y) % 2 ? 0 : pixels[index].a);
                }
            _g.draw_img(stippled, bounds);
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
                if (s.pressed) {
                    _g.draw_rect(r, true).set_ink(colors.button_bg);
                }
                linux::gemix::draw_stock_arrow(_g, r, axis, part);
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
