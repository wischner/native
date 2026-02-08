#include <stdexcept>
#include <windows.h>

#include <native.h>
#include "gpx_wnd.h"
#include "globals.h"

static void apply_gdi_state(HDC hdc, native::gpx_wnd *self, win::wingpx *cache)
{
    if (!cache)
        return;

    // Set pen if color or thickness changed
    if (cache->current_fg != self->ink() || cache->current_thickness != self->pen())
    {
        if (cache->pen)
            DeleteObject(cache->pen);

        native::rgba c = self->ink();
        COLORREF color = RGB(c.r, c.g, c.b);
        cache->pen = CreatePen(PS_SOLID, self->pen(), color);
        SelectObject(hdc, cache->pen);

        cache->current_fg = self->ink();
        cache->current_thickness = self->pen();
    }

    // Set brush
    if (cache->brush)
        DeleteObject(cache->brush);

    native::rgba c = self->ink();
    COLORREF color = RGB(c.r, c.g, c.b);
    cache->brush = CreateSolidBrush(color);
    SelectObject(hdc, cache->brush);

    // Set clip region
    HRGN rgn = CreateRectRgn(
        self->clip().p.x,
        self->clip().p.y,
        self->clip().x2() + 1,
        self->clip().y2() + 1);
    SelectClipRgn(hdc, rgn);
    DeleteObject(rgn);
}

namespace native
{

    gpx_wnd::gpx_wnd(const wnd *window, point offset)
        : _wnd(const_cast<wnd *>(window)), _offset(offset)
    {
        HWND hwnd = win::wnd_bindings.from_b(_wnd);
        if (!hwnd)
            throw std::runtime_error("Windows: No HWND available for gpx_wnd");
    }

    gpx_wnd::~gpx_wnd() = default;

    gpx &gpx_wnd::set_clip(const rect &r)
    {
        _clip = r;
        return *this;
    }

    rect gpx_wnd::clip() const
    {
        return _clip;
    }

    gpx &gpx_wnd::clear(rgba color)
    {
        HWND hwnd = win::wnd_bindings.from_b(_wnd);
        HDC hdc = GetDC(hwnd);

        COLORREF c = RGB(color.r, color.g, color.b);
        HBRUSH brush = CreateSolidBrush(c);

        RECT rect = {_clip.p.x, _clip.p.y, _clip.x2() + 1, _clip.y2() + 1};
        FillRect(hdc, &rect, brush);

        DeleteObject(brush);
        ReleaseDC(hwnd, hdc);
        return *this;
    }

    gpx &gpx_wnd::draw_line(point from, point to)
    {
        HWND hwnd = win::wnd_bindings.from_b(_wnd);
        HDC hdc = GetDC(hwnd);
        auto *cache = win::wnd_gpx_bindings.from_a(_wnd);

        apply_gdi_state(hdc, this, cache);

        MoveToEx(hdc, from.x, from.y, nullptr);
        LineTo(hdc, to.x, to.y);

        ReleaseDC(hwnd, hdc);
        return *this;
    }

    gpx &gpx_wnd::draw_rect(rect r, bool filled)
    {
        HWND hwnd = win::wnd_bindings.from_b(_wnd);
        HDC hdc = GetDC(hwnd);
        auto *cache = win::wnd_gpx_bindings.from_a(_wnd);

        apply_gdi_state(hdc, this, cache);

        if (filled)
        {
            RECT rect = {r.p.x, r.p.y, r.x2() + 1, r.y2() + 1};
            FillRect(hdc, &rect, cache->brush);
        }
        else
        {
            Rectangle(hdc, r.p.x, r.p.y, r.x2() + 1, r.y2() + 1);
        }

        ReleaseDC(hwnd, hdc);
        return *this;
    }

    gpx &gpx_wnd::draw_text(const std::string &text, point p)
    {
        HWND hwnd = win::wnd_bindings.from_b(_wnd);
        HDC hdc = GetDC(hwnd);
        auto *cache = win::wnd_gpx_bindings.from_a(_wnd);

        apply_gdi_state(hdc, this, cache);

        // Create or use cached font
        if (!cache->font)
        {
            cache->font = CreateFont(
                14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("Arial"));
        }
        SelectObject(hdc, cache->font);

        // Set text color
        native::rgba c = ink();
        SetTextColor(hdc, RGB(c.r, c.g, c.b));
        SetBkMode(hdc, TRANSPARENT);

        // Draw text
        TextOutA(hdc, p.x, p.y, text.c_str(), text.length());

        ReleaseDC(hwnd, hdc);
        return *this;
    }

    gpx &gpx_wnd::draw_img(const img &src, point dst)
    {
        HWND hwnd = win::wnd_bindings.from_b(_wnd);
        HDC hdc = GetDC(hwnd);
        auto *cache = win::wnd_gpx_bindings.from_a(_wnd);

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
