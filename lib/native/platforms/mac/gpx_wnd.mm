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

    // Set stroke color if changed
    if (cache->current_fg != self->get_ink()) {
        native::rgba c = self->get_ink();
        CGContextSetRGBStrokeColor(cg_context, c.r / 255.0, c.g / 255.0, c.b / 255.0, c.a / 255.0);
        CGContextSetRGBFillColor(cg_context, c.r / 255.0, c.g / 255.0, c.b / 255.0, c.a / 255.0);
        cache->current_fg = self->get_ink();
    }

    // Set line width if changed
    if (cache->current_thickness != self->get_pen()) {
        CGContextSetLineWidth(cg_context, self->get_pen());
        cache->current_thickness = self->get_pen();
    }

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
        apply_cocoa_state(context, this, cache);
        CGContextRef cg_context = (CGContextRef)[context CGContext];

        // Draw line
        CGContextBeginPath(cg_context);
        CGContextMoveToPoint(cg_context, from.x, from.y);
        CGContextAddLineToPoint(cg_context, to.x, to.y);
        CGContextStrokePath(cg_context);

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
        apply_cocoa_state(context, this, cache);
        CGContextRef cg_context = (CGContextRef)[context CGContext];

        CGRect rect = CGRectMake(r.p.x, r.p.y, r.d.w, r.d.h);

        if (filled)
            CGContextFillRect(cg_context, rect);
        else
            CGContextStrokeRect(cg_context, rect);

        [view unlockFocus];
        [view setNeedsDisplay:YES];
        return *this;
    }

    gpx &gpx_wnd::draw_text(const std::string &text, point p) {
        auto *cache = mac::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache || !cache->view)
            return *this;

        NSView *view = cache->view;
        [view lockFocus];

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

        // Create CGImage from RGBA pixel data
        CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
        CGContextRef bitmap_context = CGBitmapContextCreate(
            const_cast<rgba *>(src.pixels()),
            src.w(), src.h(), 8, src.w() * 4,
            color_space,
            kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);

        CGImageRef cg_image = CGBitmapContextCreateImage(bitmap_context);

        NSGraphicsContext *context = [NSGraphicsContext currentContext];
        CGContextRef cg_context = (CGContextRef)[context CGContext];

        // Draw image
        CGRect rect = CGRectMake(dst.x, dst.y, src.w(), src.h());
        CGContextDrawImage(cg_context, rect, cg_image);

        // Cleanup
        CGImageRelease(cg_image);
        CGContextRelease(bitmap_context);
        CGColorSpaceRelease(color_space);

        [view unlockFocus];
        [view setNeedsDisplay:YES];
        return *this;
    }

} // namespace native
