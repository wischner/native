//
// Implements the Windows theme with native GDI control primitives and system
// colors. Image contexts use an equivalent backend-local emulation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <memory>

#include <windows.h>

#include <native.h>

#include "globals.h"

namespace
{
    class saved_state
    {
    public:
        explicit saved_state(native::gpx &g)
            : _g(g), _ink(g.get_ink()), _paper(g.get_paper()),
              _pen(g.get_pen()), _font(g.get_font()), _clip(g.get_clip()) {}

        ~saved_state() {
            _g.set_ink(_ink).set_paper(_paper).set_pen(_pen).set_font(_font);
            _g.set_clip(_clip);
        }

    private:
        native::gpx &_g;
        native::rgba _ink;
        native::rgba _paper;
        std::uint8_t _pen;
        const native::font_t &_font;
        native::rect _clip;
    };

    void select_control_font(HDC hdc) {
        SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
    }

    void apply_clip(HDC hdc, native::gpx &g) {
        const native::rect clip = g.get_clip();
        HRGN region = CreateRectRgn(
            clip.p.x,
            clip.p.y,
            clip.x2(),
            clip.y2());
        SelectClipRgn(hdc, region);
        DeleteObject(region);
    }

    class windows_theme final : public native::theme
    {
    public:
        explicit windows_theme(native::gpx &g) : theme(g) {}

        metrics defaults() const override {
            metrics m;
            m.menu_bar_height = std::max(20, GetSystemMetrics(SM_CYMENU));
            m.menu_item_height = m.menu_bar_height;
            m.text_padding_x = std::max(4, GetSystemMetrics(SM_CXEDGE) * 2);
            m.popup_width = 180;
            if (HDC hdc = GetDC(nullptr)) {
                select_control_font(hdc);
                TEXTMETRICW metric{};
                if (GetTextMetricsW(hdc, &metric)) {
                    m.popup_width = std::max(
                        120,
                        static_cast<int>(metric.tmAveCharWidth) * 24 +
                            m.text_padding_x * 2);
                }
                ReleaseDC(nullptr, hdc);
            }
            return m;
        }

        palette native_palette() const override {
            palette p;
            p.button_bg = windows::rgba_from_sys_color(COLOR_BTNFACE);
            p.button_border = windows::rgba_from_sys_color(COLOR_WINDOWFRAME);
            p.button_highlight = windows::rgba_from_sys_color(COLOR_3DHIGHLIGHT);
            p.button_shadow = windows::rgba_from_sys_color(COLOR_3DSHADOW);
            p.button_text = windows::rgba_from_sys_color(COLOR_BTNTEXT);
            p.button_disabled_text = windows::rgba_from_sys_color(COLOR_GRAYTEXT);
            p.button_hot_bg = p.button_bg;
            p.button_hot_text = p.button_text;
            p.button_pressed_bg = p.button_bg;
            p.button_pressed_text = p.button_text;
            p.menu_bar_bg = windows::rgba_from_sys_color(COLOR_MENU);
            p.menu_bar_line_top = p.button_highlight;
            p.menu_bar_line_bottom = p.button_shadow;
            p.menu_text = windows::rgba_from_sys_color(COLOR_MENUTEXT);
            p.menu_disabled_text = windows::rgba_from_sys_color(COLOR_GRAYTEXT);
            p.menu_hot_bg = windows::rgba_from_sys_color(COLOR_HIGHLIGHT);
            p.menu_hot_text = windows::rgba_from_sys_color(COLOR_HIGHLIGHTTEXT);
            p.menu_popup_bg = windows::rgba_from_sys_color(COLOR_MENU);
            p.menu_popup_border = p.button_shadow;
            return p;
        }

        theme &draw_button(
            const native::rect &r,
            const std::string &text,
            const state &s) override {
            HWND hwnd = windows::hwnd_from_gpx(_g);
            if (!hwnd)
                return draw_button_fallback(r, text, s);
            HDC hdc = GetDC(hwnd);
            if (!hdc)
                return *this;

            apply_clip(hdc, _g);
            RECT bounds = windows::to_rect(r);
            UINT flags = DFCS_BUTTONPUSH;
            if (s.pressed)
                flags |= DFCS_PUSHED;
            if (s.hot)
                flags |= DFCS_HOT;
            if (s.disabled)
                flags |= DFCS_INACTIVE;
            DrawFrameControl(hdc, &bounds, DFC_BUTTON, flags);

            if (s.pressed)
                OffsetRect(&bounds, 1, 1);
            select_control_font(hdc);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(
                hdc,
                GetSysColor(s.disabled ? COLOR_GRAYTEXT : COLOR_BTNTEXT));
            const std::wstring label = windows::utf8_to_wide(text);
            DrawTextW(
                hdc,
                label.c_str(),
                static_cast<int>(label.size()),
                &bounds,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectClipRgn(hdc, nullptr);
            ReleaseDC(hwnd, hdc);
            return *this;
        }

        theme &draw_menu_bar(const native::rect &r) override {
            HWND hwnd = windows::hwnd_from_gpx(_g);
            if (!hwnd)
                return draw_menu_bar_fallback(r);
            HDC hdc = GetDC(hwnd);
            if (!hdc)
                return *this;
            apply_clip(hdc, _g);
            RECT bounds = windows::to_rect(r);
            FillRect(hdc, &bounds, GetSysColorBrush(COLOR_MENU));
            DrawEdge(hdc, &bounds, EDGE_ETCHED, BF_BOTTOM);
            SelectClipRgn(hdc, nullptr);
            ReleaseDC(hwnd, hdc);
            return *this;
        }

        theme &draw_menu_title(
            const native::rect &r,
            const std::string &text,
            const state &s) override {
            return draw_menu_entry(r, text, s, true);
        }

        theme &draw_menu_item(
            const native::rect &r,
            const std::string &text,
            const state &s) override {
            return draw_menu_entry(r, text, s, false);
        }

        theme &draw_popup_frame(const native::rect &r) override {
            HWND hwnd = windows::hwnd_from_gpx(_g);
            if (!hwnd)
                return draw_popup_fallback(r);
            HDC hdc = GetDC(hwnd);
            if (!hdc)
                return *this;
            apply_clip(hdc, _g);
            RECT bounds = windows::to_rect(r);
            FillRect(hdc, &bounds, GetSysColorBrush(COLOR_MENU));
            DrawEdge(hdc, &bounds, EDGE_RAISED, BF_RECT);
            SelectClipRgn(hdc, nullptr);
            ReleaseDC(hwnd, hdc);
            return *this;
        }

        theme &draw_list_item(
            const native::rect &r,
            const std::string &text,
            const state &s) override {
            return draw_menu_entry(r, text, s, false);
        }

    private:
        int text_width(const std::string &text) const {
            HDC hdc = GetDC(nullptr);
            if (!hdc)
                return static_cast<int>(text.size()) * 7;
            select_control_font(hdc);
            SIZE size{};
            const std::wstring label = windows::utf8_to_wide(text);
            const BOOL measured = GetTextExtentPoint32W(
                hdc,
                label.c_str(),
                static_cast<int>(label.size()),
                &size);
            ReleaseDC(nullptr, hdc);
            return measured ? size.cx : static_cast<int>(text.size()) * 7;
        }

        int text_height() const {
            HDC hdc = GetDC(nullptr);
            if (!hdc)
                return 12;
            select_control_font(hdc);
            TEXTMETRICW metric{};
            const BOOL measured = GetTextMetricsW(hdc, &metric);
            ReleaseDC(nullptr, hdc);
            return measured ? std::max(1, static_cast<int>(metric.tmHeight)) : 12;
        }

        void draw_bevel(const native::rect &r, bool inset, const palette &p) {
            if (!r.d.w || !r.d.h)
                return;
            const native::rgba top = inset ? p.button_shadow : p.button_highlight;
            const native::rgba bottom = inset ? p.button_highlight : p.button_shadow;
            _g.set_ink(top)
                .draw_line(r.p, native::point(r.x2() - 1, r.p.y))
                .draw_line(r.p, native::point(r.p.x, r.y2() - 1));
            _g.set_ink(bottom)
                .draw_line(native::point(r.p.x, r.y2() - 1),
                           native::point(r.x2() - 1, r.y2() - 1))
                .draw_line(native::point(r.x2() - 1, r.p.y),
                           native::point(r.x2() - 1, r.y2() - 1));
            _g.set_ink(p.button_border).draw_rect(r, false);
        }

        theme &draw_button_fallback(
            const native::rect &r,
            const std::string &text,
            const state &s) {
            saved_state saved(_g);
            const palette p = native_palette();
            const int offset = s.pressed ? 1 : 0;
            _g.set_pen(1)
                .set_ink(s.pressed ? p.button_pressed_bg
                                   : (s.hot ? p.button_hot_bg : p.button_bg))
                .draw_rect(r, true);
            draw_bevel(r, s.pressed, p);
            _g.set_font(native::font_t::stock(native::font_role::control));
            _g.set_ink(s.disabled ? p.button_disabled_text : p.button_text)
                .draw_text(
                    text,
                    native::point(
                        r.p.x + std::max(
                            0,
                            (static_cast<int>(r.d.w) - text_width(text)) / 2) + offset,
                        r.p.y + std::max(
                            0,
                            (static_cast<int>(r.d.h) - text_height()) / 2) + offset));
            return *this;
        }

        theme &draw_menu_bar_fallback(const native::rect &r) {
            saved_state saved(_g);
            const palette p = native_palette();
            _g.set_pen(1).set_ink(p.menu_bar_bg).draw_rect(r, true);
            if (r.d.w && r.d.h) {
                _g.set_ink(p.menu_bar_line_bottom).draw_line(
                    native::point(r.p.x, r.y2() - 1),
                    native::point(r.x2() - 1, r.y2() - 1));
            }
            return *this;
        }

        theme &draw_menu_entry(
            const native::rect &r,
            const std::string &text,
            const state &s,
            bool title) {
            HWND hwnd = windows::hwnd_from_gpx(_g);
            const bool active = s.hot || s.selected;
            if (!hwnd)
                return draw_menu_entry_fallback(r, text, s, title);
            HDC hdc = GetDC(hwnd);
            if (!hdc)
                return *this;
            apply_clip(hdc, _g);
            RECT bounds = windows::to_rect(r);
            FillRect(
                hdc,
                &bounds,
                GetSysColorBrush(active ? COLOR_HIGHLIGHT : COLOR_MENU));
            bounds.left += defaults().text_padding_x;
            select_control_font(hdc);
            SetBkMode(hdc, TRANSPARENT);
            SetTextColor(
                hdc,
                GetSysColor(
                    s.disabled
                        ? COLOR_GRAYTEXT
                        : (active ? COLOR_HIGHLIGHTTEXT : COLOR_MENUTEXT)));
            const std::wstring label = windows::utf8_to_wide(text);
            DrawTextW(
                hdc,
                label.c_str(),
                static_cast<int>(label.size()),
                &bounds,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            SelectClipRgn(hdc, nullptr);
            ReleaseDC(hwnd, hdc);
            (void)title;
            return *this;
        }

        theme &draw_menu_entry_fallback(
            const native::rect &r,
            const std::string &text,
            const state &s,
            bool title) {
            saved_state saved(_g);
            const palette p = native_palette();
            const bool active = s.hot || s.selected;
            _g.set_pen(1)
                .set_ink(active ? p.menu_hot_bg
                                : (title ? p.menu_bar_bg : p.menu_popup_bg))
                .draw_rect(r, true);
            _g.set_font(native::font_t::stock(native::font_role::control));
            _g.set_ink(
                  s.disabled
                      ? p.menu_disabled_text
                      : (active ? p.menu_hot_text : p.menu_text))
                .draw_text(
                    text,
                    native::point(
                        r.p.x + defaults().text_padding_x,
                        r.p.y + std::max(
                            0,
                            (static_cast<int>(r.d.h) - text_height()) / 2)));
            return *this;
        }

        theme &draw_popup_fallback(const native::rect &r) {
            saved_state saved(_g);
            const palette p = native_palette();
            _g.set_pen(1).set_ink(p.menu_popup_bg).draw_rect(r, true);
            _g.set_ink(p.menu_popup_border).draw_rect(r, false);
            return *this;
        }
    };
}

namespace native
{
    std::unique_ptr<theme> theme::create(gpx &painter) {
        return std::make_unique<windows_theme>(painter);
    }
}
