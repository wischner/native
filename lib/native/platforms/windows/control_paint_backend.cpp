//
// Implements the Windows control-paint backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <string>

#include <windows.h>

#include <native.h>

#include "../../control_paint_backend.h"
#include "globals.h"

namespace native
{
namespace detail
{
    control_paint::metrics control_paint_backend_metrics() {
        control_paint::metrics m;

        int menu_h = GetSystemMetrics(SM_CYMENU);
        if (menu_h <= 0)
            menu_h = 20;

        int edge = GetSystemMetrics(SM_CXEDGE);
        if (edge <= 0)
            edge = 2;

        m.menu_bar_height = menu_h;
        m.menu_item_height = menu_h;
        m.text_padding_x = edge * 2;

        int avg_char_w = 7;
        if (HDC hdc = GetDC(nullptr)) {
            HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            HFONT old = static_cast<HFONT>(SelectObject(hdc, font));
            TEXTMETRIC tm{};
            if (GetTextMetrics(hdc, &tm))
                avg_char_w = std::max(1, static_cast<int>(tm.tmAveCharWidth));
            if (old)
                SelectObject(hdc, old);
            ReleaseDC(nullptr, hdc);
        }
        m.popup_width = avg_char_w * 24 + (m.text_padding_x * 2);
        return m;
    }

    control_paint::palette control_paint_backend_palette() {
        control_paint::palette p;
        p.button_bg = windows::rgba_from_sys_color(COLOR_BTNFACE);
        p.button_border = windows::rgba_from_sys_color(COLOR_BTNSHADOW);
        p.button_text = windows::rgba_from_sys_color(COLOR_BTNTEXT);
        p.button_hot_bg = windows::rgba_from_sys_color(COLOR_BTNFACE);
        p.button_hot_text = windows::rgba_from_sys_color(COLOR_BTNTEXT);
        p.button_pressed_bg = windows::rgba_from_sys_color(COLOR_BTNFACE);
        p.button_pressed_text = windows::rgba_from_sys_color(COLOR_BTNTEXT);

        p.menu_bar_bg = windows::rgba_from_sys_color(COLOR_MENU);
        p.menu_bar_line_top = windows::rgba_from_sys_color(COLOR_3DHIGHLIGHT);
        p.menu_bar_line_bottom = windows::rgba_from_sys_color(COLOR_3DSHADOW);
        p.menu_text = windows::rgba_from_sys_color(COLOR_MENUTEXT);
        p.menu_hot_bg = windows::rgba_from_sys_color(COLOR_HIGHLIGHT);
        p.menu_hot_text = windows::rgba_from_sys_color(COLOR_HIGHLIGHTTEXT);
        p.menu_popup_bg = windows::rgba_from_sys_color(COLOR_MENU);
        p.menu_popup_border = windows::rgba_from_sys_color(COLOR_3DSHADOW);
        return p;
    }

    int control_paint_backend_text_width(const std::string &text) {
        if (HDC hdc = GetDC(nullptr)) {
            SIZE sz{};
            std::wstring wide = windows::utf8_to_wide(text);
            HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            HFONT old = static_cast<HFONT>(SelectObject(hdc, font));
            const BOOL ok = GetTextExtentPoint32W(hdc, wide.c_str(),
                                                  static_cast<int>(wide.size()), &sz);
            if (old)
                SelectObject(hdc, old);
            ReleaseDC(nullptr, hdc);
            if (ok)
                return sz.cx;
        }
        return static_cast<int>(text.size()) * 7;
    }

    int control_paint_backend_text_y_centered(const rect &r) {
        int text_h = 12;
        if (HDC hdc = GetDC(nullptr)) {
            TEXTMETRIC tm{};
            HFONT font = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
            HFONT old = static_cast<HFONT>(SelectObject(hdc, font));
            if (GetTextMetrics(hdc, &tm))
                text_h = std::max(1, static_cast<int>(tm.tmHeight));
            if (old)
                SelectObject(hdc, old);
            ReleaseDC(nullptr, hdc);
        }
        return r.p.y + std::max(0, (static_cast<int>(r.d.h) - text_h) / 2);
    }

    bool control_paint_backend_draw_button_face_native(
        gpx &g,
        const rect &r,
        const control_paint::state &) {
        HWND hwnd = windows::hwnd_from_gpx(g);
        if (!hwnd)
            return false;
        HDC hdc = GetDC(hwnd);
        if (!hdc)
            return false;

        RECT rc = windows::to_rect(r);
        InflateRect(&rc, -1, -1);
        FillRect(hdc, &rc, GetSysColorBrush(COLOR_BTNFACE));
        ReleaseDC(hwnd, hdc);
        return true;
    }

    bool control_paint_backend_draw_button_frame_native(
        gpx &g,
        const rect &r,
        const control_paint::state &s) {
        HWND hwnd = windows::hwnd_from_gpx(g);
        if (!hwnd)
            return false;
        HDC hdc = GetDC(hwnd);
        if (!hdc)
            return false;

        RECT rc = windows::to_rect(r);
        UINT edge = s.pressed ? EDGE_SUNKEN : EDGE_RAISED;
        DrawEdge(hdc, &rc, edge, BF_RECT);
        ReleaseDC(hwnd, hdc);
        return true;
    }

    bool control_paint_backend_draw_button_text_native(
        gpx &g,
        const rect &r,
        const std::string &text,
        const control_paint::state &s) {
        HWND hwnd = windows::hwnd_from_gpx(g);
        if (!hwnd)
            return false;
        HDC hdc = GetDC(hwnd);
        if (!hdc)
            return false;

        RECT rc = windows::to_rect(r);
        std::wstring wide = windows::utf8_to_wide(text);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, GetSysColor(s.disabled ? COLOR_GRAYTEXT : COLOR_BTNTEXT));
        DrawTextW(hdc, wide.c_str(), -1, &rc,
                  DT_SINGLELINE | DT_VCENTER | DT_CENTER | DT_END_ELLIPSIS);
        ReleaseDC(hwnd, hdc);
        return true;
    }

    bool control_paint_backend_draw_menu_bar_background_native(
        gpx &g,
        const rect &r) {
        HWND hwnd = windows::hwnd_from_gpx(g);
        if (!hwnd)
            return false;
        HDC hdc = GetDC(hwnd);
        if (!hdc)
            return false;

        RECT rc = windows::to_rect(r);
        FillRect(hdc, &rc, GetSysColorBrush(COLOR_MENU));
        ReleaseDC(hwnd, hdc);
        return true;
    }

    bool control_paint_backend_draw_menu_item_background_native(
        gpx &g,
        const rect &r,
        const control_paint::state &s) {
        HWND hwnd = windows::hwnd_from_gpx(g);
        if (!hwnd)
            return false;
        HDC hdc = GetDC(hwnd);
        if (!hdc)
            return false;

        RECT rc = windows::to_rect(r);
        const bool active = s.selected || s.hot;
        FillRect(hdc, &rc, GetSysColorBrush(active ? COLOR_HIGHLIGHT : COLOR_MENU));
        ReleaseDC(hwnd, hdc);
        return true;
    }

    bool control_paint_backend_draw_menu_item_text_native(
        gpx &g,
        const rect &r,
        const std::string &text,
        const control_paint::state &s) {
        HWND hwnd = windows::hwnd_from_gpx(g);
        if (!hwnd)
            return false;
        HDC hdc = GetDC(hwnd);
        if (!hdc)
            return false;

        RECT rc = windows::to_rect(r);
        rc.left += control_paint_backend_metrics().text_padding_x;

        const bool active = s.selected || s.hot;
        int color_id = COLOR_MENUTEXT;
        if (s.disabled)
            color_id = COLOR_GRAYTEXT;
        else if (active)
            color_id = COLOR_HIGHLIGHTTEXT;

        std::wstring wide = windows::utf8_to_wide(text);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, GetSysColor(color_id));
        DrawTextW(hdc, wide.c_str(), -1, &rc,
                  DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_END_ELLIPSIS);
        ReleaseDC(hwnd, hdc);
        return true;
    }
}
}
