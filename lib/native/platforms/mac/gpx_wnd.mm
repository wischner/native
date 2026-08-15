//
// Implements the macOS window-graphics backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#import <Cocoa/Cocoa.h>
#include <stdexcept>

#include <native.h>
#include "gpx_wnd.h"
#include "globals.h"

static void apply_cocoa_state(
    NSGraphicsContext *context,
    native::gpx_wnd *self,
    mac::mac_gpx *cache) {
    if (!context || !cache)
        return;

    CGContextRef cg_context = (CGContextRef)[context CGContext];

    native::rgba c = self->get_ink();
    CGContextSetRGBStrokeColor(
        cg_context,
        c.r / 255.0,
        c.g / 255.0,
        c.b / 255.0,
        c.a / 255.0);
    CGContextSetRGBFillColor(
        cg_context,
        c.r / 255.0,
        c.g / 255.0,
        c.b / 255.0,
        c.a / 255.0);
    CGContextSetLineWidth(cg_context, self->get_pen());
    cache->current_fg = self->get_ink();
    cache->current_thickness = self->get_pen();

    // Set clip rectangle
    CGRect clip_rect = CGRectMake(
        self->get_clip().p.x,
        self->get_clip().p.y,
        self->get_clip().d.w,
        self->get_clip().d.h);
    CGContextClipToRect(cg_context, clip_rect);
}

namespace native
{

    gpx_wnd::gpx_wnd(const wnd *window, point offset)
        : _wnd(const_cast<wnd *>(window)), _offset(offset) {
        NSWindow *nswin = mac::wnd_bindings.handle_from_object(_wnd);
        if (!nswin)
            throw std::runtime_error("macOS: No NSWindow available for gpx_wnd");

        // Get or create cache
        auto *cache = mac::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache) {
            cache = new mac::mac_gpx();
            cache->view = [nswin contentView];
            mac::wnd_gpx_bindings.register_pair(_wnd, cache);
        }
        const size dimensions = window->get_dimensions();
        _clip = rect(0, 0, dimensions.w, dimensions.h);
    }

    gpx_wnd::~gpx_wnd() {
        auto *cache = mac::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache)
            return;

        // The NSWindow owns the cached view.
        delete cache;
        mac::wnd_gpx_bindings.unregister_by_handle(_wnd);
    }

    gpx &gpx_wnd::set_clip(const rect &r) {
        _clip = r;
        return *this;
    }

    rect gpx_wnd::get_clip() const {
        return _clip;
    }

    gpx &gpx_wnd::clear(rgba color) {
        auto *cache = mac::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache || !cache->view)
            return *this;

        NSView *view = cache->view;
        [view lockFocus];

        NSGraphicsContext *context = [NSGraphicsContext currentContext];
        CGContextRef cg_context = (CGContextRef)[context CGContext];
        CGContextSaveGState(cg_context);
        apply_cocoa_state(context, this, cache);

        // Set fill color
        CGContextSetRGBFillColor(
            cg_context,
            color.r / 255.0,
            color.g / 255.0,
            color.b / 255.0,
            color.a / 255.0);

        // Fill rectangle
        CGRect rect = CGRectMake(_clip.p.x, _clip.p.y, _clip.d.w, _clip.d.h);
        CGContextFillRect(cg_context, rect);
        CGContextRestoreGState(cg_context);

        [view unlockFocus];
        [view setNeedsDisplay:YES];
        return *this;
    }

    gpx &gpx_wnd::draw_line(point from, point to) {
        auto *cache = mac::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache || !cache->view)
            return *this;

        NSView *view = cache->view;
        [view lockFocus];

        NSGraphicsContext *context = [NSGraphicsContext currentContext];
        CGContextRef cg_context = (CGContextRef)[context CGContext];
        CGContextSaveGState(cg_context);
        apply_cocoa_state(context, this, cache);

        // Draw line
        CGContextBeginPath(cg_context);
        CGContextMoveToPoint(cg_context, from.x, from.y);
        CGContextAddLineToPoint(cg_context, to.x, to.y);
        CGContextStrokePath(cg_context);
        CGContextRestoreGState(cg_context);

        [view unlockFocus];
        [view setNeedsDisplay:YES];
        return *this;
    }

    gpx &gpx_wnd::draw_rect(rect r, bool filled) {
        auto *cache = mac::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache || !cache->view)
            return *this;

        NSView *view = cache->view;
        [view lockFocus];

        NSGraphicsContext *context = [NSGraphicsContext currentContext];
        CGContextRef cg_context = (CGContextRef)[context CGContext];
        CGContextSaveGState(cg_context);
        apply_cocoa_state(context, this, cache);

        CGRect rect = CGRectMake(r.p.x, r.p.y, r.d.w, r.d.h);

        if (filled)
            CGContextFillRect(cg_context, rect);
        else
            CGContextStrokeRect(cg_context, rect);
        CGContextRestoreGState(cg_context);

        [view unlockFocus];
        [view setNeedsDisplay:YES];
        return *this;
    }

    gpx &gpx_wnd::draw_text(const std::string &text, point p) {
        if (_font && !_font->valid())
            return *this;
        auto *cache = mac::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache || !cache->view)
            return *this;

        NSView *view = cache->view;
        [view lockFocus];

        NSGraphicsContext *context = [NSGraphicsContext currentContext];
        CGContextRef cg_context = (CGContextRef)[context CGContext];
        CGContextSaveGState(cg_context);
        apply_cocoa_state(context, this, cache);

        NSString *ns_text = [NSString stringWithUTF8String:text.c_str()];

        native::rgba c = get_ink();
        NSColor *color = [NSColor colorWithRed:c.r / 255.0
                                        green:c.g / 255.0
                                         blue:c.b / 255.0
                                        alpha:c.a / 255.0];

        auto *fh = mac::font_bindings.object_from_handle(get_font().id());
        NSFont *nsfont = fh ? fh->ns_font : [NSFont systemFontOfSize:[NSFont systemFontSize]];

        NSDictionary *attributes = @{
            NSForegroundColorAttributeName: color,
            NSFontAttributeName: nsfont
        };

        [ns_text drawAtPoint:NSMakePoint(p.x, p.y) withAttributes:attributes];
        CGContextRestoreGState(cg_context);

        [view unlockFocus];
        [view setNeedsDisplay:YES];
        return *this;
    }

    gpx &gpx_wnd::draw_img(const img &src, point dst) {
        auto *cache = mac::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache || !cache->view)
            return *this;

        NSView *view = cache->view;
        [view lockFocus];

        // Create a straight-alpha CGImage from Native RGBA pixel data.
        CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
        CGDataProviderRef provider = CGDataProviderCreateWithData(
            nullptr,
            src.pixels(),
            static_cast<std::size_t>(src.w()) * src.h() * sizeof(rgba),
            nullptr);
        CGImageRef cg_image = provider
            ? CGImageCreate(
                  src.w(), src.h(), 8, 32, src.w() * sizeof(rgba),
                  color_space,
                  static_cast<CGBitmapInfo>(kCGImageAlphaLast) |
                      static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Big),
                  provider,
                  nullptr,
                  false,
                  kCGRenderingIntentDefault)
            : nullptr;
        if (provider)
            CGDataProviderRelease(provider);
        if (!cg_image) {
            CGColorSpaceRelease(color_space);
            [view unlockFocus];
            return *this;
        }

        NSGraphicsContext *context = [NSGraphicsContext currentContext];
        CGContextRef cg_context = (CGContextRef)[context CGContext];
        CGContextSaveGState(cg_context);
        apply_cocoa_state(context, this, cache);

        // Draw image
        CGRect rect = CGRectMake(dst.x, dst.y, src.w(), src.h());
        CGContextDrawImage(cg_context, rect, cg_image);
        CGContextRestoreGState(cg_context);

        // Cleanup
        CGImageRelease(cg_image);
        CGColorSpaceRelease(color_space);

        [view unlockFocus];
        [view setNeedsDisplay:YES];
        return *this;
    }

} // namespace native
