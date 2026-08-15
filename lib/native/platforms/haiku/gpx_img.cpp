//
// Implements the Haiku image-graphics backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <vector>

#include <Application.h>
#include <View.h>
#include <Bitmap.h>
#include <Region.h>
#include <String.h>

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
        // Create BBitmap for text rendering
        BRect bounds(0, 0, _img.w() - 1, _img.h() - 1);
        BBitmap *bitmap = new BBitmap(bounds, B_RGBA32, true);

        if (!bitmap || !bitmap->IsValid()) {
            if (bitmap)
                delete bitmap;
            return *this;
        }

        std::vector<std::uint8_t> before(
            static_cast<std::size_t>(_img.w()) * _img.h() * 4);
        auto *bitmap_bytes = static_cast<std::uint8_t *>(bitmap->Bits());
        for (int y = 0; y < _img.h(); ++y) {
            auto *row = bitmap_bytes + y * bitmap->BytesPerRow();
            for (int x = 0; x < _img.w(); ++x) {
                const std::size_t index =
                    static_cast<std::size_t>(y) * _img.w() + x;
                const rgba pixel = _img.pixels()[index];
                before[index * 4] = pixel.b;
                before[index * 4 + 1] = pixel.g;
                before[index * 4 + 2] = pixel.r;
                before[index * 4 + 3] = pixel.a;
                std::memcpy(row + x * 4, before.data() + index * 4, 4);
            }
        }

        // Get view from bitmap
        BView *view = new BView(bounds, "offscreen", B_FOLLOW_NONE, B_WILL_DRAW);
        bitmap->AddChild(view);

        if (bitmap->Lock()) {
            // Set text color
            rgb_color color = {_ink.r, _ink.g, _ink.b, _ink.a};
            view->SetHighColor(color);

            auto *font = haiku::font_bindings.object_from_handle(
                get_font().id());
            if (font)
                view->SetFont(&font->bfont);

            // Set clip region
            BRect clip_rect(
                _clip.p.x,
                _clip.p.y,
                _clip.x2() - 1,
                _clip.y2() - 1);
            BRegion region(clip_rect);
            view->ConstrainClippingRegion(&region);

            // Draw text
            view->DrawString(
                text.c_str(),
                BPoint(p.x, p.y + get_font_metrics().ascent));
            view->Sync();

            rgba *destination = const_cast<rgba *>(_img.pixels());
            const auto *base = static_cast<const std::uint8_t *>(
                bitmap->Bits());
            for (int y = 0; y < _img.h(); ++y) {
                const auto *row = base + y * bitmap->BytesPerRow();
                for (int x = 0; x < _img.w(); ++x) {
                    const std::size_t index =
                        static_cast<std::size_t>(y) * _img.w() + x;
                    const bool changed =
                        row[x * 4] != before[index * 4] ||
                        row[x * 4 + 1] != before[index * 4 + 1] ||
                        row[x * 4 + 2] != before[index * 4 + 2];
                    destination[index] = rgba(
                        row[x * 4 + 2],
                        row[x * 4 + 1],
                        row[x * 4],
                        changed ? _ink.a : before[index * 4 + 3]);
                }
            }

            bitmap->Unlock();
        }

        delete bitmap;
        return *this;
    }

    gpx &gpx_img::draw_img(const img &src, point dst) {
        detail::copy_image(_img, _clip, src, dst);
        return *this;
    }

} // namespace native
