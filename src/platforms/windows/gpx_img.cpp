#include <stdexcept>
#include <algorithm>
#include <windows.h>

#include <native.h>
#include "gpx_img.h"
#include "globals.h"

namespace native
{

    gpx_img::gpx_img(const img &image)
        : _img(image)
    {
        // No dependencies needed for software rendering
    }

    gpx &gpx_img::set_clip(const rect &r)
    {
        _clip = r;
        return *this;
    }

    rect gpx_img::clip() const
    {
        return _clip;
    }

    gpx &gpx_img::clear(rgba color)
    {
        rgba *pixels = const_cast<rgba *>(_img.pixels());
        int x1 = std::max(0, static_cast<int>(_clip.p.x));
        int y1 = std::max(0, static_cast<int>(_clip.p.y));
        int x2 = std::min(static_cast<int>(_img.w() - 1), static_cast<int>(_clip.x2()));
        int y2 = std::min(static_cast<int>(_img.h() - 1), static_cast<int>(_clip.y2()));

        for (int y = y1; y <= y2; ++y)
            for (int x = x1; x <= x2; ++x)
                pixels[y * _img.w() + x] = color;
        return *this;
    }

    gpx &gpx_img::draw_line(point from, point to)
    {
        // Bresenham's line algorithm
        int x0 = from.x, y0 = from.y, x1 = to.x, y1 = to.y;
        int dx = abs(x1 - x0), dy = abs(y1 - y0);
        int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
        int err = dx - dy;

        rgba *pixels = const_cast<rgba *>(_img.pixels());
        int clip_x1 = _clip.p.x, clip_y1 = _clip.p.y;
        int clip_x2 = _clip.x2(), clip_y2 = _clip.y2();

        while (true)
        {
            if (x0 >= clip_x1 && x0 <= clip_x2 && y0 >= clip_y1 && y0 <= clip_y2 &&
                x0 >= 0 && x0 < _img.w() && y0 >= 0 && y0 < _img.h())
            {
                pixels[y0 * _img.w() + x0] = _ink;
            }
            if (x0 == x1 && y0 == y1)
                break;
            int e2 = 2 * err;
            if (e2 > -dy)
            {
                err -= dy;
                x0 += sx;
            }
            if (e2 < dx)
            {
                err += dx;
                y0 += sy;
            }
        }
        return *this;
    }

    gpx &gpx_img::draw_rect(rect r, bool filled)
    {
        int x1 = std::max(0, std::max(static_cast<int>(r.p.x), static_cast<int>(_clip.p.x)));
        int y1 = std::max(0, std::max(static_cast<int>(r.p.y), static_cast<int>(_clip.p.y)));
        int x2 = std::min(static_cast<int>(_img.w() - 1), std::min(static_cast<int>(r.x2()), static_cast<int>(_clip.x2())));
        int y2 = std::min(static_cast<int>(_img.h() - 1), std::min(static_cast<int>(r.y2()), static_cast<int>(_clip.y2())));

        if (x1 > x2 || y1 > y2)
            return *this;

        rgba *pixels = const_cast<rgba *>(_img.pixels());

        if (filled)
        {
            for (int y = y1; y <= y2; ++y)
                for (int x = x1; x <= x2; ++x)
                    pixels[y * _img.w() + x] = _ink;
        }
        else
        {
            for (int x = x1; x <= x2; ++x)
            {
                pixels[y1 * _img.w() + x] = _ink;
                pixels[y2 * _img.w() + x] = _ink;
            }
            for (int y = y1 + 1; y < y2; ++y)
            {
                pixels[y * _img.w() + x1] = _ink;
                pixels[y * _img.w() + x2] = _ink;
            }
        }
        return *this;
    }

    gpx &gpx_img::draw_text(const std::string &text, point p)
    {
        // Create memory DC for text rendering
        HDC hdc = CreateCompatibleDC(nullptr);
        if (!hdc)
            return *this;

        // Create DIB section for our buffer
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = _img.w();
        bmi.bmiHeader.biHeight = -_img.h(); // Top-down
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;

        void *bits = nullptr;
        HBITMAP hbm = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!hbm)
        {
            DeleteDC(hdc);
            return *this;
        }

        // Copy our pixel data to the DIB
        memcpy(bits, _img.pixels(), _img.w() * _img.h() * 4);

        SelectObject(hdc, hbm);

        // Create font
        HFONT font = CreateFont(
            14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, TEXT("Arial"));
        SelectObject(hdc, font);

        // Set clip region
        HRGN rgn = CreateRectRgn(_clip.p.x, _clip.p.y, _clip.x2() + 1, _clip.y2() + 1);
        SelectClipRgn(hdc, rgn);

        // Set text color and draw
        SetTextColor(hdc, RGB(_ink.r, _ink.g, _ink.b));
        SetBkMode(hdc, TRANSPARENT);
        TextOutA(hdc, p.x, p.y, text.c_str(), text.length());

        // Copy result back to our buffer
        memcpy(const_cast<rgba *>(_img.pixels()), bits, _img.w() * _img.h() * 4);

        // Cleanup
        DeleteObject(rgn);
        DeleteObject(font);
        DeleteObject(hbm);
        DeleteDC(hdc);

        return *this;
    }

    gpx &gpx_img::draw_img(const img &src, point dst)
    {
        rgba *dst_pixels = const_cast<rgba *>(_img.pixels());
        const rgba *src_pixels = src.pixels();

        int clip_x1 = _clip.p.x, clip_y1 = _clip.p.y;
        int clip_x2 = _clip.x2(), clip_y2 = _clip.y2();

        for (coord y = 0; y < src.h() && (dst.y + y) < _img.h(); ++y)
        {
            for (coord x = 0; x < src.w() && (dst.x + x) < _img.w(); ++x)
            {
                int dst_x = dst.x + x, dst_y = dst.y + y;
                if (dst_x >= clip_x1 && dst_x <= clip_x2 && dst_y >= clip_y1 && dst_y <= clip_y2)
                {
                    dst_pixels[dst_y * _img.w() + dst_x] = src_pixels[y * src.w() + x];
                }
            }
        }
        return *this;
    }

} // namespace native
