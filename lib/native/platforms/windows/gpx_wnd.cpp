//
// Implements the Windows window-graphics backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>
#include <windows.h>

#include <native.h>
#include "gpx_wnd.h"
#include "globals.h"

static void apply_gdi_state(HDC hdc, native::gpx_wnd *self, windows::win_gpx *cache) {
    if (!cache)
        return;

    // Set pen if color or thickness changed
    if (cache->current_fg != self->get_ink() || cache->current_thickness != self->get_pen()) {
        if (cache->pen)
            DeleteObject(cache->pen);

        native::rgba c = self->get_ink();
        COLORREF color = RGB(c.r, c.g, c.b);
        cache->pen = CreatePen(PS_SOLID, self->get_pen(), color);
        SelectObject(hdc, cache->pen);

        cache->current_fg = self->get_ink();
        cache->current_thickness = self->get_pen();
    }

    // Set brush
    if (cache->brush)
        DeleteObject(cache->brush);

    native::rgba c = self->get_ink();
    COLORREF color = RGB(c.r, c.g, c.b);
    cache->brush = CreateSolidBrush(color);
    SelectObject(hdc, cache->brush);

    // Set clip region
    HRGN rgn = CreateRectRgn(
        self->get_clip().p.x,
        self->get_clip().p.y,
        self->get_clip().x2() + 1,
        self->get_clip().y2() + 1);
    SelectClipRgn(hdc, rgn);
    DeleteObject(rgn);
}

namespace native
{

    gpx_wnd::gpx_wnd(const wnd *window, point offset)
        : _wnd(const_cast<wnd *>(window)), _offset(offset) {
        HWND hwnd = windows::wnd_bindings.handle_from_object(_wnd);
        if (!hwnd)
            throw std::runtime_error("Windows: No HWND available for gpx_wnd");

        if (!windows::wnd_gpx_bindings.object_from_handle(_wnd))
            windows::wnd_gpx_bindings.register_pair(_wnd, new windows::win_gpx());
    }

    gpx_wnd::~gpx_wnd() {
        auto *cache =
            windows::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache)
            return;

        if (cache->pen)
            DeleteObject(cache->pen);
        if (cache->brush)
            DeleteObject(cache->brush);
        delete cache;
        windows::wnd_gpx_bindings.unregister_by_handle(_wnd);
    }

    gpx &gpx_wnd::set_clip(const rect &r) {
        _clip = r;
        return *this;
    }

    rect gpx_wnd::get_clip() const {
        return _clip;
    }

    gpx &gpx_wnd::clear(rgba color) {
        HWND hwnd = windows::wnd_bindings.handle_from_object(_wnd);
        HDC hdc = GetDC(hwnd);

        COLORREF c = RGB(color.r, color.g, color.b);
        HBRUSH brush = CreateSolidBrush(c);

        RECT rect = {_clip.p.x, _clip.p.y, _clip.x2() + 1, _clip.y2() + 1};
        FillRect(hdc, &rect, brush);

        DeleteObject(brush);
        ReleaseDC(hwnd, hdc);
        return *this;
    }

    gpx &gpx_wnd::draw_line(point from, point to) {
        HWND hwnd = windows::wnd_bindings.handle_from_object(_wnd);
        HDC hdc = GetDC(hwnd);
        auto *cache = windows::wnd_gpx_bindings.object_from_handle(_wnd);

        apply_gdi_state(hdc, this, cache);

        MoveToEx(hdc, from.x, from.y, nullptr);
        LineTo(hdc, to.x, to.y);

        ReleaseDC(hwnd, hdc);
        return *this;
    }

    gpx &gpx_wnd::draw_rect(rect r, bool filled) {
        HWND hwnd = windows::wnd_bindings.handle_from_object(_wnd);
        HDC hdc = GetDC(hwnd);
        auto *cache = windows::wnd_gpx_bindings.object_from_handle(_wnd);

        apply_gdi_state(hdc, this, cache);

        if (filled) {
            RECT rect = {r.p.x, r.p.y, r.x2() + 1, r.y2() + 1};
            FillRect(hdc, &rect, cache->brush);
        }
        else {
            Rectangle(hdc, r.p.x, r.p.y, r.x2() + 1, r.y2() + 1);
        }

        ReleaseDC(hwnd, hdc);
        return *this;
    }

    gpx &gpx_wnd::draw_text(const std::string &text, point p) {
        HWND hwnd = windows::wnd_bindings.handle_from_object(_wnd);
        HDC hdc = GetDC(hwnd);
        auto *cache = windows::wnd_gpx_bindings.object_from_handle(_wnd);

        apply_gdi_state(hdc, this, cache);

        auto *fh = windows::font_bindings.object_from_handle(get_font().id());
        if (fh && fh->hfont)
            SelectObject(hdc, fh->hfont);

        native::rgba c = get_ink();
        SetTextColor(hdc, RGB(c.r, c.g, c.b));
        SetBkMode(hdc, TRANSPARENT);

        TextOutA(hdc, p.x, p.y, text.c_str(), text.length());

        ReleaseDC(hwnd, hdc);
        return *this;
    }

    gpx &gpx_wnd::draw_img(const img &src, point dst) {
        HWND hwnd = windows::wnd_bindings.handle_from_object(_wnd);
        HDC hdc = GetDC(hwnd);
        auto *cache = windows::wnd_gpx_bindings.object_from_handle(_wnd);

        apply_gdi_state(hdc, this, cache);

        // Create DIB from RGBA pixel data
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = src.w();
        bmi.bmiHeader.biHeight = -src.h(); // Top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        // Draw DIB directly
        StretchDIBits(
            hdc,
            dst.x, dst.y, src.w(), src.h(),
            0, 0, src.w(), src.h(),
            src.pixels(),
            &bmi,
            DIB_RGB_COLORS,
            SRCCOPY);

        ReleaseDC(hwnd, hdc);
        return *this;
    }

} // namespace native
