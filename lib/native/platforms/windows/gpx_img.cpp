//
// Implements the Windows image-graphics backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <vector>
#include <windows.h>

#include <native.h>
#include "gpx_img.h"
#include "globals.h"
#include "../../software_image.h"

namespace native
{

    gpx_img::gpx_img(const img &image)
        : _img(image), _clip(0, 0, image.w(), image.h()) {
        // No dependencies needed for software rendering
    }

    gpx &gpx_img::set_clip(const rect &r) {
        _clip = r;
        return *this;
    }

    rect gpx_img::get_clip() const {
        return _clip;
    }

    gpx &gpx_img::clear(rgba color) {
        detail::clear_image(_img, _clip, color);
        return *this;
    }

    gpx &gpx_img::draw_line(point from, point to) {
        detail::draw_image_line(
            _img, _clip, from, to, _ink, _thickness);
        return *this;
    }

    gpx &gpx_img::draw_rect(rect r, bool filled) {
        detail::draw_image_rect(
            _img, _clip, r, _ink, _thickness, filled);
        return *this;
    }

    gpx &gpx_img::draw_text(const std::string &text, point p) {
        if (_font && !_font->valid())
            return *this;
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
        if (!hbm) {
            DeleteDC(hdc);
            return *this;
        }

        std::vector<std::uint8_t> before(
            static_cast<std::size_t>(_img.w()) * _img.h() * 4);
        for (std::size_t index = 0;
             index < static_cast<std::size_t>(_img.w()) * _img.h();
             ++index) {
            before[index * 4] = _img.pixels()[index].b;
            before[index * 4 + 1] = _img.pixels()[index].g;
            before[index * 4 + 2] = _img.pixels()[index].r;
            before[index * 4 + 3] = _img.pixels()[index].a;
        }
        memcpy(bits, before.data(), before.size());

        HGDIOBJ previous_bitmap = SelectObject(hdc, hbm);

        auto *font = windows::font_bindings.object_from_handle(
            get_font().id());
        HGDIOBJ previous_font = SelectObject(
            hdc,
            font && font->hfont
                ? font->hfont
                : GetStockObject(DEFAULT_GUI_FONT));

        // Set clip region
        HRGN rgn = CreateRectRgn(
            _clip.p.x, _clip.p.y, _clip.x2(), _clip.y2());
        SelectClipRgn(hdc, rgn);

        // Set text color and draw
        SetTextColor(hdc, RGB(_ink.r, _ink.g, _ink.b));
        SetBkMode(hdc, TRANSPARENT);
        const std::wstring wide = windows::utf8_to_wide(text);
        TextOutW(
            hdc,
            p.x,
            p.y,
            wide.data(),
            static_cast<int>(wide.size()));

        const auto *after = static_cast<const std::uint8_t *>(bits);
        rgba *destination = const_cast<rgba *>(_img.pixels());
        for (std::size_t index = 0;
             index < static_cast<std::size_t>(_img.w()) * _img.h();
             ++index) {
            const bool changed = after[index * 4] != before[index * 4] ||
                after[index * 4 + 1] != before[index * 4 + 1] ||
                after[index * 4 + 2] != before[index * 4 + 2];
            destination[index] = rgba(
                after[index * 4 + 2],
                after[index * 4 + 1],
                after[index * 4],
                changed ? _ink.a : before[index * 4 + 3]);
        }

        // Cleanup
        DeleteObject(rgn);
        SelectObject(hdc, previous_font);
        SelectObject(hdc, previous_bitmap);
        DeleteObject(hbm);
        DeleteDC(hdc);

        return *this;
    }

    gpx &gpx_img::draw_img(const img &src, point dst) {
        detail::copy_image(_img, _clip, src, dst);
        return *this;
    }

} // namespace native
