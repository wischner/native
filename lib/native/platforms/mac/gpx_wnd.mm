//
// Implements the macOS window-graphics backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "gpx_wnd.h"

#include <stdexcept>
#include <vector>

#import <Cocoa/Cocoa.h>

#include <native.h>

#include "globals.h"

namespace
{
    // Apply portable drawing state to the active AppKit paint context.
    void apply_cocoa_state(NSGraphicsContext *context,
                           native::gpx_wnd *self,
                           mac::mac_gpx *cache) {
        if (!context || !cache)
            return;

        CGContextRef cg_context =
            static_cast<CGContextRef>([context CGContext]);

        native::rgba color = self->get_ink();
        CGContextSetRGBStrokeColor(cg_context,
                                   color.r / 255.0,
                                   color.g / 255.0,
                                   color.b / 255.0,
                                   color.a / 255.0);
        CGContextSetRGBFillColor(cg_context,
                                 color.r / 255.0,
                                 color.g / 255.0,
                                 color.b / 255.0,
                                 color.a / 255.0);
        CGContextSetLineWidth(cg_context, self->get_pen());
        cache->current_fg = self->get_ink();
        cache->current_thickness = self->get_pen();

        const native::rect clip = self->get_clip();
        CGContextClipToRect(
            cg_context,
            CGRectMake(clip.p.x, clip.p.y, clip.d.w, clip.d.h));
    }

    // Save and prepare the current drawRect graphics state.
    CGContextRef begin_cocoa_state(native::gpx_wnd *self,
                                    mac::mac_gpx *cache) {
        NSGraphicsContext *context =
            [NSGraphicsContext currentContext];
        if (!context)
            return nullptr;

        CGContextRef cg_context =
            static_cast<CGContextRef>([context CGContext]);
        if (!cg_context)
            return nullptr;

        CGContextSaveGState(cg_context);
        apply_cocoa_state(context, self, cache);
        return cg_context;
    }
}

namespace native
{

    gpx_wnd::gpx_wnd(const wnd *window, point offset)
        : _wnd(const_cast<wnd *>(window))
        , _offset(offset) {
        NSView *control_view = mac::view_from_control(_wnd);
        NSWindow *nswin = mac::wnd_bindings.handle_from_object(_wnd);
        if (!nswin && control_view)
            nswin = [control_view window];
        if (!nswin)
            throw std::runtime_error(
                "macOS: No NSWindow available for gpx_wnd");

        // Get or create cache
        auto *cache = mac::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache) {
            cache = new mac::mac_gpx();
            cache->view = control_view ? control_view
                                       : [nswin contentView];
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
        if (!cache)
            return *this;

        CGContextRef cg_context = begin_cocoa_state(this, cache);
        if (!cg_context)
            return *this;

        CGContextSetRGBFillColor(cg_context,
                                 color.r / 255.0,
                                 color.g / 255.0,
                                 color.b / 255.0,
                                 color.a / 255.0);

        const CGRect area =
            CGRectMake(_clip.p.x, _clip.p.y, _clip.d.w, _clip.d.h);
        CGContextFillRect(cg_context, area);
        CGContextRestoreGState(cg_context);
        return *this;
    }

    gpx &gpx_wnd::draw_line(point from, point to) {
        auto *cache = mac::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache)
            return *this;

        CGContextRef cg_context = begin_cocoa_state(this, cache);
        if (!cg_context)
            return *this;

        CGContextBeginPath(cg_context);
        CGContextMoveToPoint(cg_context, from.x, from.y);
        CGContextAddLineToPoint(cg_context, to.x, to.y);
        CGContextStrokePath(cg_context);
        CGContextRestoreGState(cg_context);
        return *this;
    }

    gpx &gpx_wnd::draw_rect(rect r, bool filled) {
        auto *cache = mac::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache)
            return *this;

        CGContextRef cg_context = begin_cocoa_state(this, cache);
        if (!cg_context)
            return *this;

        const CGRect area =
            CGRectMake(r.p.x, r.p.y, r.d.w, r.d.h);

        if (filled)
            CGContextFillRect(cg_context, area);
        else
            CGContextStrokeRect(cg_context, area);
        CGContextRestoreGState(cg_context);
        return *this;
    }

    gpx &gpx_wnd::draw_native_text(const std::string &text, point p) {
        if (_font && !_font->valid())
            return *this;
        auto *cache = mac::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache)
            return *this;

        CGContextRef cg_context = begin_cocoa_state(this, cache);
        if (!cg_context)
            return *this;

        NSString *ns_text =
            [NSString stringWithUTF8String:text.c_str()];

        native::rgba c = get_ink();
        NSColor *color = [NSColor colorWithRed:c.r / 255.0
                                         green:c.g / 255.0
                                          blue:c.b / 255.0
                                         alpha:c.a / 255.0];

        auto *fh =
            mac::font_bindings.object_from_handle(get_font().id());
        NSFont *nsfont =
            fh ? fh->ns_font
               : [NSFont systemFontOfSize:[NSFont systemFontSize]];

        NSDictionary *attributes = @{
            NSForegroundColorAttributeName : color,
            NSFontAttributeName : nsfont
        };

        [ns_text drawAtPoint:NSMakePoint(p.x, p.y)
              withAttributes:attributes];
        CGContextRestoreGState(cg_context);
        return *this;
    }

    gpx &gpx_wnd::draw_img(const img &src, point dst) {
        auto *cache = mac::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache)
            return *this;

        // Core Graphics bitmap contexts require premultiplied color.
        // Portable images deliberately store straight RGBA, so convert
        // before asking AppKit to composite the result.
        std::vector<rgba> pixels(
            static_cast<std::size_t>(src.w()) * src.h());
        for (std::size_t index = 0; index < pixels.size(); ++index) {
            const rgba source = src.pixels()[index];
            pixels[index] = rgba(
                static_cast<std::uint8_t>(
                    (static_cast<unsigned>(source.r) * source.a +
                     127U) /
                    255U),
                static_cast<std::uint8_t>(
                    (static_cast<unsigned>(source.g) * source.a +
                     127U) /
                    255U),
                static_cast<std::uint8_t>(
                    (static_cast<unsigned>(source.b) * source.a +
                     127U) /
                    255U),
                source.a);
        }
        CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
        CGDataProviderRef provider = CGDataProviderCreateWithData(
            nullptr,
            pixels.data(),
            pixels.size() * sizeof(rgba),
            nullptr);
        const CGBitmapInfo alpha_info =
            static_cast<CGBitmapInfo>(
                kCGImageAlphaPremultipliedLast);
        CGImageRef cg_image =
            provider ? CGImageCreate(src.w(),
                                     src.h(),
                                     8,
                                     32,
                                     src.w() * sizeof(rgba),
                                     color_space,
                                     alpha_info |
                                         static_cast<CGBitmapInfo>(
                                             kCGBitmapByteOrder32Big),
                                     provider,
                                     nullptr,
                                     false,
                                     kCGRenderingIntentDefault)
                     : nullptr;
        if (provider)
            CGDataProviderRelease(provider);
        if (!cg_image) {
            CGColorSpaceRelease(color_space);
            return *this;
        }

        CGContextRef cg_context = begin_cocoa_state(this, cache);
        if (!cg_context) {
            CGImageRelease(cg_image);
            CGColorSpaceRelease(color_space);
            return *this;
        }

        NSImage *image = [[NSImage alloc]
            initWithCGImage:cg_image
                       size:NSMakeSize(src.w(), src.h())];
        [image drawInRect:NSMakeRect(dst.x, dst.y, src.w(), src.h())
                 fromRect:NSZeroRect
                operation:NSCompositingOperationSourceOver
                 fraction:1.0
           respectFlipped:YES
                    hints:nil];
        [image release];
        CGContextRestoreGState(cg_context);

        // Cleanup
        CGImageRelease(cg_image);
        CGColorSpaceRelease(color_space);
        return *this;
    }

} // namespace native
