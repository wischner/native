#import <AppKit/AppKit.h>

#include <stdexcept>

#include <native.h>

#include "gpx_wnd.h"
#include "globals.h"

namespace
{
    NSColor *to_color(native::rgba c)
    {
        return [NSColor colorWithCalibratedRed:c.r / 255.0
                                         green:c.g / 255.0
                                          blue:c.b / 255.0
                                         alpha:c.a / 255.0];
    }

    void apply_state(native::gpx_wnd *self, gnustep::gnustepgpx *cache)
    {
        if (!cache)
            return;

        if (cache->current_fg != self->ink())
        {
            NSColor *c = to_color(self->ink());
            [c setStroke];
            [c setFill];
            cache->current_fg = self->ink();
        }

        if (cache->current_thickness != self->pen())
            cache->current_thickness = self->pen();
    }

    void apply_clip(const native::rect &clip)
    {
        NSRect r = NSMakeRect(clip.p.x, clip.p.y, clip.d.w, clip.d.h);
        [[NSBezierPath bezierPathWithRect:r] addClip];
    }
}

namespace native
{

gpx_wnd::gpx_wnd(const wnd *window, point offset)
    : _wnd(const_cast<wnd *>(window)), _offset(offset)
{
    NSView *view = gnustep::view_bindings.from_b(_wnd);
    if (!view)
        throw std::runtime_error("GNUstep: No NSView available for gpx_wnd.");

    auto *cache = gnustep::wnd_gpx_bindings.from_a(_wnd);
    if (!cache)
    {
        cache = new gnustep::gnustepgpx();
        cache->view = view;
        gnustep::wnd_gpx_bindings.register_pair(_wnd, cache);
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
    auto *cache = gnustep::wnd_gpx_bindings.from_a(_wnd);
    if (!cache || !cache->view)
        return *this;

    if (![NSGraphicsContext currentContext])
        return *this;

    [NSGraphicsContext saveGraphicsState];
    apply_clip(_clip);
    [to_color(color) setFill];
    NSRectFill(NSMakeRect(_clip.p.x, _clip.p.y, _clip.d.w, _clip.d.h));
    [NSGraphicsContext restoreGraphicsState];

    cache->current_fg = color;
    return *this;
}

gpx &gpx_wnd::draw_line(point from, point to)
{
    auto *cache = gnustep::wnd_gpx_bindings.from_a(_wnd);
    if (!cache || !cache->view)
        return *this;

    if (![NSGraphicsContext currentContext])
        return *this;

    [NSGraphicsContext saveGraphicsState];
    apply_clip(_clip);
    apply_state(this, cache);

    NSBezierPath *path = [NSBezierPath bezierPath];
    [path setLineWidth:pen()];
    [path moveToPoint:NSMakePoint(from.x, from.y)];
    [path lineToPoint:NSMakePoint(to.x, to.y)];
    [path stroke];

    [NSGraphicsContext restoreGraphicsState];
    return *this;
}

gpx &gpx_wnd::draw_rect(rect r, bool filled)
{
    auto *cache = gnustep::wnd_gpx_bindings.from_a(_wnd);
    if (!cache || !cache->view)
        return *this;

    if (![NSGraphicsContext currentContext])
        return *this;

    [NSGraphicsContext saveGraphicsState];
    apply_clip(_clip);
    apply_state(this, cache);

    NSRect rr = NSMakeRect(r.p.x, r.p.y, r.d.w, r.d.h);
    NSBezierPath *path = [NSBezierPath bezierPathWithRect:rr];
    [path setLineWidth:pen()];

    if (filled)
        [path fill];
    else
        [path stroke];

    [NSGraphicsContext restoreGraphicsState];
    return *this;
}

gpx &gpx_wnd::draw_text(const std::string &text, point p)
{
    auto *cache = gnustep::wnd_gpx_bindings.from_a(_wnd);
    if (!cache || !cache->view)
        return *this;

    if (![NSGraphicsContext currentContext])
        return *this;

    [NSGraphicsContext saveGraphicsState];
    apply_clip(_clip);

    NSString *ns_text = [NSString stringWithUTF8String:text.c_str()];
    NSDictionary *attrs = [NSDictionary dictionaryWithObjectsAndKeys:
        to_color(ink()), NSForegroundColorAttributeName,
        [NSFont systemFontOfSize:12], NSFontAttributeName,
        nil];
    [ns_text drawAtPoint:NSMakePoint(p.x, p.y) withAttributes:attrs];

    [NSGraphicsContext restoreGraphicsState];
    return *this;
}

gpx &gpx_wnd::draw_img(const img &src, point dst)
{
    auto *cache = gnustep::wnd_gpx_bindings.from_a(_wnd);
    if (!cache || !cache->view)
        return *this;

    if (![NSGraphicsContext currentContext])
        return *this;

    [NSGraphicsContext saveGraphicsState];
    apply_clip(_clip);

    unsigned char *planes[5] = {reinterpret_cast<unsigned char *>(const_cast<rgba *>(src.pixels())), nullptr, nullptr, nullptr, nullptr};
    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc]
        initWithBitmapDataPlanes:planes
                      pixelsWide:src.w()
                      pixelsHigh:src.h()
                   bitsPerSample:8
                 samplesPerPixel:4
                        hasAlpha:YES
                        isPlanar:NO
                  colorSpaceName:NSDeviceRGBColorSpace
                    bytesPerRow:src.w() * 4
                   bitsPerPixel:32];

    if (rep)
    {
        NSImage *img_obj = [[NSImage alloc] initWithSize:NSMakeSize(src.w(), src.h())];
        [img_obj addRepresentation:rep];
        [img_obj drawInRect:NSMakeRect(dst.x, dst.y, src.w(), src.h())
                   fromRect:NSMakeRect(0, 0, src.w(), src.h())
                  operation:NSCompositeSourceOver
                   fraction:1.0];
        [img_obj release];
        [rep release];
    }

    [NSGraphicsContext restoreGraphicsState];
    return *this;
}

} // namespace native
