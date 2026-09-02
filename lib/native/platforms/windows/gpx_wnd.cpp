//
// Implements the Windows window-graphics backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <cstring>
#include <stdexcept>
#include <vector>
#include <windows.h>

#include <native.h>
#include "gpx_wnd.h"
#include "globals.h"

static void apply_gdi_state(HDC hdc,
                            native::gpx_wnd *self,
                            windows::win_gpx *cache) {
    if (!cache)
        return;

    // Set pen if color or thickness changed
    if (cache->current_fg != self->get_ink() ||
        cache->current_thickness != self->get_pen()) {
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
    HRGN rgn = CreateRectRgn(self->get_clip().p.x,
                             self->get_clip().p.y,
                             self->get_clip().x2(),
                             self->get_clip().y2());
    SelectClipRgn(hdc, rgn);
    DeleteObject(rgn);
}

namespace native
{

    gpx_wnd::gpx_wnd(const wnd *window, point offset)
        : _wnd(const_cast<wnd *>(window))
        , _offset(offset) {
        HWND hwnd = windows::wnd_bindings.handle_from_object(_wnd);
        if (!hwnd)
            throw std::runtime_error(
                "Windows: No HWND available for gpx_wnd");

        if (!windows::wnd_gpx_bindings.object_from_handle(_wnd))
            windows::wnd_gpx_bindings.register_pair(
                _wnd, new windows::win_gpx());
        const size dimensions = window->get_dimensions();
        _clip = rect(0, 0, dimensions.w, dimensions.h);
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
        HDC hdc = windows::acquire_gpx_dc(*this);
        if (!hdc)
            return *this;

        COLORREF c = RGB(color.r, color.g, color.b);
        HBRUSH brush = CreateSolidBrush(c);

        RECT rect = {_clip.p.x, _clip.p.y, _clip.x2(), _clip.y2()};
        FillRect(hdc, &rect, brush);

        DeleteObject(brush);
        windows::release_gpx_dc(*this, hdc);
        return *this;
    }

    gpx &gpx_wnd::draw_line(point from, point to) {
        HDC hdc = windows::acquire_gpx_dc(*this);
        if (!hdc)
            return *this;
        auto *cache =
            windows::wnd_gpx_bindings.object_from_handle(_wnd);

        apply_gdi_state(hdc, this, cache);

        MoveToEx(hdc, from.x, from.y, nullptr);
        LineTo(hdc, to.x, to.y);

        windows::release_gpx_dc(*this, hdc);
        return *this;
    }

    gpx &gpx_wnd::draw_rect(rect r, bool filled) {
        HDC hdc = windows::acquire_gpx_dc(*this);
        if (!hdc)
            return *this;
        auto *cache =
            windows::wnd_gpx_bindings.object_from_handle(_wnd);

        apply_gdi_state(hdc, this, cache);

        if (filled) {
            RECT rect = {r.p.x, r.p.y, r.x2(), r.y2()};
            FillRect(hdc, &rect, cache->brush);
        } else {
            HGDIOBJ previous_brush = SelectObject(
                hdc, GetStockObject(NULL_BRUSH));
            Rectangle(hdc, r.p.x, r.p.y, r.x2(), r.y2());
            SelectObject(hdc, previous_brush);
        }

        windows::release_gpx_dc(*this, hdc);
        return *this;
    }

    gpx &gpx_wnd::draw_native_text(const std::string &text, point p) {
        if (_font && !_font->valid())
            return *this;
        HDC hdc = windows::acquire_gpx_dc(*this);
        if (!hdc)
            return *this;
        auto *cache =
            windows::wnd_gpx_bindings.object_from_handle(_wnd);

        apply_gdi_state(hdc, this, cache);

        auto *fh =
            windows::font_bindings.object_from_handle(get_font().id());
        if (fh && fh->hfont)
            SelectObject(hdc, fh->hfont);

        native::rgba c = get_ink();
        SetTextColor(hdc, RGB(c.r, c.g, c.b));
        SetBkMode(hdc, TRANSPARENT);

        const std::wstring wide = windows::utf8_to_wide(text);
        TextOutW(
            hdc, p.x, p.y, wide.data(), static_cast<int>(wide.size()));

        windows::release_gpx_dc(*this, hdc);
        return *this;
    }

    gpx &gpx_wnd::draw_img(const img &src, point dst) {
        HDC hdc = windows::acquire_gpx_dc(*this);
        if (!hdc)
            return *this;
        auto *cache =
            windows::wnd_gpx_bindings.object_from_handle(_wnd);

        apply_gdi_state(hdc, this, cache);

        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = src.w();
        bmi.bmiHeader.biHeight = -src.h(); // Top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        std::vector<std::uint8_t> bgra(
            static_cast<std::size_t>(src.w()) * src.h() * 4);
        bool opaque = true;
        for (std::size_t index = 0;
             index < static_cast<std::size_t>(src.w()) * src.h();
             ++index) {
            const rgba pixel = src.pixels()[index];
            bgra[index * 4] = static_cast<std::uint8_t>(
                (static_cast<unsigned>(pixel.b) * pixel.a + 127U) /
                255U);
            bgra[index * 4 + 1] = static_cast<std::uint8_t>(
                (static_cast<unsigned>(pixel.g) * pixel.a + 127U) /
                255U);
            bgra[index * 4 + 2] = static_cast<std::uint8_t>(
                (static_cast<unsigned>(pixel.r) * pixel.a + 127U) /
                255U);
            bgra[index * 4 + 3] = pixel.a;
            if (pixel.a != 255)
                opaque = false;
        }
        if (opaque) {
            StretchDIBits(hdc,
                          dst.x,
                          dst.y,
                          src.w(),
                          src.h(),
                          0,
                          0,
                          src.w(),
                          src.h(),
                          bgra.data(),
                          &bmi,
                          DIB_RGB_COLORS,
                          SRCCOPY);
        } else {
            void *dib_pixels = nullptr;
            HBITMAP bitmap = CreateDIBSection(
                hdc,
                &bmi,
                DIB_RGB_COLORS,
                &dib_pixels,
                nullptr,
                0);
            if (!bitmap || !dib_pixels) {
                if (bitmap)
                    DeleteObject(bitmap);
                windows::release_gpx_dc(*this, hdc);
                return *this;
            }
            std::memcpy(dib_pixels, bgra.data(), bgra.size());
            HDC memory_dc = CreateCompatibleDC(hdc);
            HGDIOBJ previous = SelectObject(memory_dc, bitmap);
            BLENDFUNCTION blend = {
                AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
            AlphaBlend(hdc,
                       dst.x,
                       dst.y,
                       src.w(),
                       src.h(),
                       memory_dc,
                       0,
                       0,
                       src.w(),
                       src.h(),
                       blend);
            SelectObject(memory_dc, previous);
            DeleteDC(memory_dc);
            DeleteObject(bitmap);
        }

        windows::release_gpx_dc(*this, hdc);
        return *this;
    }

} // namespace native
