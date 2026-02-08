#import <Cocoa/Cocoa.h>
#include <stdexcept>

#include <native.h>
#include "gpx_wnd.h"
#include "globals.h"

static void apply_cocoa_state(NSGraphicsContext *context, native::gpx_wnd *self, mac::macgpx *cache)
{
    if (!context || !cache)
        return;

    CGContextRef cgContext = (CGContextRef)[context CGContext];

    // Set stroke color if changed
    if (cache->current_fg != self->ink())
    {
        native::rgba c = self->ink();
        CGContextSetRGBStrokeColor(cgContext, c.r / 255.0, c.g / 255.0, c.b / 255.0, c.a / 255.0);
        CGContextSetRGBFillColor(cgContext, c.r / 255.0, c.g / 255.0, c.b / 255.0, c.a / 255.0);
        cache->current_fg = self->ink();
    }

    // Set line width if changed
    if (cache->current_thickness != self->pen())
    {
        CGContextSetLineWidth(cgContext, self->pen());
        cache->current_thickness = self->pen();
    }

    // Set clip rectangle
    CGRect clip_rect = CGRectMake(
        self->clip().p.x,
        self->clip().p.y,
        self->clip().d.w,
        self->clip().d.h);
    CGContextClipToRect(cgContext, clip_rect);
}

namespace native
{

    gpx_wnd::gpx_wnd(const wnd *window, point offset)
        : _wnd(const_cast<wnd *>(window)), _offset(offset)
    {
        NSWindow *nswin = mac::wnd_bindings.from_b(_wnd);
        if (!nswin)
            throw std::runtime_error("macOS: No NSWindow available for gpx_wnd");

        // Get or create cache
        auto *cache = mac::wnd_gpx_bindings.from_a(_wnd);
        if (!cache)
        {
            cache = new mac::macgpx();
            cache->view = [nswin contentView];
            mac::wnd_gpx_bindings.register_pair(_wnd, cache);
        }
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
        auto *cache = mac::wnd_gpx_bindings.from_a(_wnd);
        if (!cache || !cache->view)
            return *this;

        NSView *view = cache->view;
        [view lockFocus];

        NSGraphicsContext *context = [NSGraphicsContext currentContext];
        CGContextRef cgContext = (CGContextRef)[context CGContext];

        // Set fill color
        CGContextSetRGBFillColor(cgContext, color.r / 255.0, color.g / 255.0, color.b / 255.0, color.a / 255.0);

        // Fill rectangle
        CGRect rect = CGRectMake(_clip.p.x, _clip.p.y, _clip.d.w, _clip.d.h);
        CGContextFillRect(cgContext, rect);

        [view unlockFocus];
        [view setNeedsDisplay:YES];
        return *this;
    }

    gpx &gpx_wnd::draw_line(point from, point to)
    {
        auto *cache = mac::wnd_gpx_bindings.from_a(_wnd);
        if (!cache || !cache->view)
            return *this;

        NSView *view = cache->view;
        [view lockFocus];

        NSGraphicsContext *context = [NSGraphicsContext currentContext];
        apply_cocoa_state(context, this, cache);
        CGContextRef cgContext = (CGContextRef)[context CGContext];

        // Draw line
        CGContextBeginPath(cgContext);
        CGContextMoveToPoint(cgContext, from.x, from.y);
        CGContextAddLineToPoint(cgContext, to.x, to.y);
        CGContextStrokePath(cgContext);

        [view unlockFocus];
        [view setNeedsDisplay:YES];
        return *this;
    }

    gpx &gpx_wnd::draw_rect(rect r, bool filled)
    {
        auto *cache = mac::wnd_gpx_bindings.from_a(_wnd);
        if (!cache || !cache->view)
            return *this;

        NSView *view = cache->view;
        [view lockFocus];

        NSGraphicsContext *context = [NSGraphicsContext currentContext];
        apply_cocoa_state(context, this, cache);
        CGContextRef cgContext = (CGContextRef)[context CGContext];

        CGRect rect = CGRectMake(r.p.x, r.p.y, r.d.w, r.d.h);

        if (filled)
            CGContextFillRect(cgContext, rect);
        else
            CGContextStrokeRect(cgContext, rect);

        [view unlockFocus];
        [view setNeedsDisplay:YES];
        return *this;
    }

    gpx &gpx_wnd::draw_text(const std::string &text, point p)
    {
        auto *cache = mac::wnd_gpx_bindings.from_a(_wnd);
        if (!cache || !cache->view)
            return *this;

        NSView *view = cache->view;
        [view lockFocus];

        // Convert C++ string to NSString
        NSString *nsText = [NSString stringWithUTF8String:text.c_str()];

        // Set text attributes
        native::rgba c = ink();
        NSColor *color = [NSColor colorWithRed:c.r/255.0 green:c.g/255.0 blue:c.b/255.0 alpha:c.a/255.0];
        NSDictionary *attributes = @{
            NSForegroundColorAttributeName: color,
            NSFontAttributeName: [NSFont systemFontOfSize:12]
        };

        // Draw text
        [nsText drawAtPoint:NSMakePoint(p.x, p.y) withAttributes:attributes];

        [view unlockFocus];
        [view setNeedsDisplay:YES];
        return *this;
    }

    gpx &gpx_wnd::draw_img(const img &src, point dst)
    {
        auto *cache = mac::wnd_gpx_bindings.from_a(_wnd);
        if (!cache || !cache->view)
            return *this;

        NSView *view = cache->view;
        [view lockFocus];

        // Create CGImage from RGBA pixel data
        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        CGContextRef bitmapContext = CGBitmapContextCreate(
            const_cast<rgba *>(src.pixels()),
            src.w(), src.h(), 8, src.w() * 4,
            colorSpace,
            kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);

        CGImageRef cgImage = CGBitmapContextCreateImage(bitmapContext);

        NSGraphicsContext *context = [NSGraphicsContext currentContext];
        CGContextRef cgContext = (CGContextRef)[context CGContext];

        // Draw image
        CGRect rect = CGRectMake(dst.x, dst.y, src.w(), src.h());
        CGContextDrawImage(cgContext, rect, cgImage);

        // Cleanup
        CGImageRelease(cgImage);
        CGContextRelease(bitmapContext);
        CGColorSpaceRelease(colorSpace);

        [view unlockFocus];
        [view setNeedsDisplay:YES];
        return *this;
    }

} // namespace native
