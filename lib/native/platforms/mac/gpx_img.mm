//
// Implements the macOS image-graphics backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <Cocoa/Cocoa.h>
#include <stdexcept>
#include <algorithm>

#include <native.h>
#include "gpx_img.h"
#include "globals.h"
#include "../../software_image.h"

namespace native
{

    gpx_img::gpx_img(const img &image)
        : _img(image)
        , _clip(0, 0, image.w(), image.h()) {
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

    gpx &gpx_img::draw_native_text(const std::string &text, point p) {
        if (text.empty() || (_font && !_font->valid()))
            return *this;
        const rect clip = _clip.intersect(rect(0, 0, _img.w(), _img.h()));
        if (!clip.d.w || !clip.d.h)
            return *this;

        NSString *ns_text = [NSString stringWithUTF8String:text.c_str()];
        if (!ns_text)
            return *this;
        auto *binding = mac::font_bindings.object_from_handle(get_font().id());
        NSFont *font = binding && binding->ns_font
            ? binding->ns_font : [NSFont systemFontOfSize:0];
        NSDictionary *attributes = @{
            NSForegroundColorAttributeName: [NSColor colorWithRed:_ink.r / 255.0
                green:_ink.g / 255.0 blue:_ink.b / 255.0 alpha:_ink.a / 255.0],
            NSFontAttributeName: font
        };

        // Quartz needs premultiplied pixels. Render a transparent layer,
        // leaving the target's straight-RGBA pixels untouched until blending.
        img layer(clip.d.w, clip.d.h);
        const rect layer_bounds(0, 0, clip.d.w, clip.d.h);
        detail::clear_image(layer, layer_bounds, rgba(0, 0, 0, 0));
        auto *pixels = const_cast<rgba *>(layer.pixels());
        CGColorSpaceRef space = CGColorSpaceCreateDeviceRGB();
        CGContextRef context = CGBitmapContextCreate(pixels,
            layer.w(), layer.h(), 8, layer.w() * 4, space,
            static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) |
                static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Big));
        CGColorSpaceRelease(space);
        if (!context)
            return *this;

        // A flipped AppKit context also needs a flipped Quartz transform.
        // The flag alone leaves glyphs mirrored in the top-down pixel rows.
        CGContextTranslateCTM(context, 0, layer.h());
        CGContextScaleCTM(context, 1, -1);
        NSGraphicsContext *graphics = [NSGraphicsContext
            graphicsContextWithCGContext:context flipped:YES];
        [NSGraphicsContext saveGraphicsState];
        [NSGraphicsContext setCurrentContext:graphics];
        [ns_text drawAtPoint:NSMakePoint(p.x - clip.p.x, p.y - clip.p.y)
            withAttributes:attributes];
        [NSGraphicsContext restoreGraphicsState];
        CGContextRelease(context);

        for (std::size_t index = 0; index < static_cast<std::size_t>(layer.w()) * layer.h(); ++index) {
            auto &pixel = pixels[index];
            if (!pixel.a) continue;
            const auto straight = [alpha = pixel.a](std::uint8_t channel) {
                return static_cast<std::uint8_t>(std::min(255U,
                    (static_cast<unsigned>(channel) * 255U + alpha / 2U) / alpha));
            };
            pixel.r = straight(pixel.r);
            pixel.g = straight(pixel.g);
            pixel.b = straight(pixel.b);
        }
        detail::copy_image(_img, clip, layer, clip.p);
        return *this;
    }

    gpx &gpx_img::draw_img(const img &src, point dst) {
        detail::copy_image(_img, _clip, src, dst);
        return *this;
    }

} // namespace native
