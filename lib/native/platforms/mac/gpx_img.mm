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
        if (_font && !_font->valid())
            return *this;
        // Create CGBitmapContext from our RGBA buffer
        CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
        CGContextRef context = CGBitmapContextCreate(
            const_cast<rgba *>(_img.pixels()),
            _img.w(),
            _img.h(),
            8,
            _img.w() * 4,
            color_space,
            static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) |
                static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Big));

        if (!context) {
            CGColorSpaceRelease(color_space);
            return *this;
        }

        // Set clip region
        CGRect clip_rect =
            CGRectMake(_clip.p.x, _clip.p.y, _clip.d.w, _clip.d.h);
        CGContextClipToRect(context, clip_rect);

        // Convert text to NSString and draw
        NSString *ns_text =
            [NSString stringWithUTF8String:text.c_str()];
        NSColor *color = [NSColor colorWithRed:_ink.r / 255.0
                                         green:_ink.g / 255.0
                                          blue:_ink.b / 255.0
                                         alpha:_ink.a / 255.0];
        auto *font_binding =
            mac::font_bindings.object_from_handle(get_font().id());
        NSFont *font =
            font_binding && font_binding->ns_font
                ? font_binding->ns_font
                : [NSFont systemFontOfSize:[NSFont systemFontSize]];
        NSDictionary *attributes = @{
            NSForegroundColorAttributeName : color,
            NSFontAttributeName : font
        };

        NSGraphicsContext *ns_context =
            [NSGraphicsContext graphicsContextWithCGContext:context
                                                    flipped:YES];
        [NSGraphicsContext saveGraphicsState];
        [NSGraphicsContext setCurrentContext:ns_context];

        [ns_text drawAtPoint:NSMakePoint(p.x, p.y)
              withAttributes:attributes];

        [NSGraphicsContext restoreGraphicsState];

        // Cleanup
        CGContextRelease(context);
        CGColorSpaceRelease(color_space);

        return *this;
    }

    gpx &gpx_img::draw_img(const img &src, point dst) {
        detail::copy_image(_img, _clip, src, dst);
        return *this;
    }

} // namespace native
