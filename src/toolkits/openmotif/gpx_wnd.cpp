#include <stdexcept>

#include <Xm/Xm.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <native.h>

#include "gpx_wnd.h"
#include "globals.h"

namespace
{
    Colormap widget_colormap(Widget widget)
    {
        Colormap colormap = DefaultColormapOfScreen(XtScreen(widget));
        XtVaGetValues(widget, XtNcolormap, &colormap, nullptr);
        return colormap;
    }

    Pixel rgba_to_pixel(Widget widget, native::rgba color)
    {
        if (!widget || !motif::cached_display)
            return 0;

        XColor xc = {};
        xc.red = static_cast<unsigned short>(color.r) * 257;
        xc.green = static_cast<unsigned short>(color.g) * 257;
        xc.blue = static_cast<unsigned short>(color.b) * 257;

        Colormap colormap = widget_colormap(widget);
        if (XAllocColor(motif::cached_display, colormap, &xc))
            return xc.pixel;

        return BlackPixelOfScreen(XtScreen(widget));
    }

    native::rgba pixel_to_rgba(Widget widget, Pixel pixel)
    {
        if (!widget || !motif::cached_display)
            return native::rgba(0, 0, 0, 255);

        XColor xc = {};
        xc.pixel = pixel;
        XQueryColor(motif::cached_display, widget_colormap(widget), &xc);
        return native::rgba(
            static_cast<uint8_t>(xc.red >> 8),
            static_cast<uint8_t>(xc.green >> 8),
            static_cast<uint8_t>(xc.blue >> 8),
            255);
    }

    void apply_gc(Widget widget, native::gpx_wnd *self, motif::motifgpx *cache)
    {
        if (!widget || !cache || !cache->gc)
            return;

        if (cache->current_fg != self->ink())
        {
            XSetForeground(motif::cached_display, cache->gc, rgba_to_pixel(widget, self->ink()));
            cache->current_fg = self->ink();
        }

        if (cache->current_thickness != self->pen())
        {
            XSetLineAttributes(motif::cached_display, cache->gc, self->pen(), LineSolid, CapButt, JoinMiter);
            cache->current_thickness = self->pen();
        }
    }
} // namespace

namespace native
{

    gpx_wnd::gpx_wnd(const wnd *window, point offset)
        : _wnd(const_cast<wnd *>(window)), _offset(offset)
    {
        if (!motif::cached_display)
            throw std::runtime_error("Motif: No display available for gpx_wnd.");

        Widget canvas = motif::wnd_bindings.from_b(_wnd);
        if (!canvas || !XtIsRealized(canvas))
            throw std::runtime_error("Motif: Drawing widget is not realized.");

        auto *cache = motif::wnd_gpx_bindings.from_a(_wnd);
        if (!cache)
        {
            cache = new motif::motifgpx();
            cache->gc = XCreateGC(motif::cached_display, XtWindow(canvas), 0, nullptr);

            XWindowAttributes attrs;
            XGetWindowAttributes(motif::cached_display, XtWindow(canvas), &attrs);
            cache->backbuffer = XCreatePixmap(
                motif::cached_display,
                XtWindow(canvas),
                static_cast<unsigned int>(attrs.width),
                static_cast<unsigned int>(attrs.height),
                static_cast<unsigned int>(attrs.depth));
            cache->buf_w = attrs.width;
            cache->buf_h = attrs.height;

            Pixel background = WhitePixelOfScreen(XtScreen(canvas));
            Pixel foreground = BlackPixelOfScreen(XtScreen(canvas));
            XtVaGetValues(canvas, XmNbackground, &background, XmNforeground, &foreground, nullptr);

            set_paper(pixel_to_rgba(canvas, background));
            set_ink(pixel_to_rgba(canvas, foreground));

            XSetForeground(motif::cached_display, cache->gc, background);
            XFillRectangle(
                motif::cached_display,
                cache->backbuffer,
                cache->gc,
                0, 0,
                static_cast<unsigned int>(cache->buf_w),
                static_cast<unsigned int>(cache->buf_h));
            cache->current_fg = paper();

            motif::wnd_gpx_bindings.register_pair(_wnd, cache);
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
        auto *cache = motif::wnd_gpx_bindings.from_a(_wnd);
        Widget canvas = motif::wnd_bindings.from_b(_wnd);
        if (!cache || !cache->backbuffer || !canvas)
            return *this;

        XSetForeground(motif::cached_display, cache->gc, rgba_to_pixel(canvas, color));
        XFillRectangle(
            motif::cached_display,
            cache->backbuffer,
            cache->gc,
            _clip.p.x, _clip.p.y,
            static_cast<unsigned int>(_clip.d.w),
            static_cast<unsigned int>(_clip.d.h));
        cache->current_fg = color;
        return *this;
    }

    gpx &gpx_wnd::draw_line(point from, point to)
    {
        auto *cache = motif::wnd_gpx_bindings.from_a(_wnd);
        Widget canvas = motif::wnd_bindings.from_b(_wnd);
        if (!cache || !cache->backbuffer || !canvas)
            return *this;

        apply_gc(canvas, this, cache);
        XDrawLine(motif::cached_display, cache->backbuffer, cache->gc, from.x, from.y, to.x, to.y);
        return *this;
    }

    gpx &gpx_wnd::draw_rect(rect r, bool filled)
    {
        auto *cache = motif::wnd_gpx_bindings.from_a(_wnd);
        Widget canvas = motif::wnd_bindings.from_b(_wnd);
        if (!cache || !cache->backbuffer || !canvas)
            return *this;

        apply_gc(canvas, this, cache);

        if (filled)
        {
            XFillRectangle(motif::cached_display, cache->backbuffer, cache->gc, r.p.x, r.p.y, r.d.w, r.d.h);
        }
        else
        {
            XDrawRectangle(motif::cached_display, cache->backbuffer, cache->gc, r.p.x, r.p.y, r.d.w - 1, r.d.h - 1);
        }

        return *this;
    }

    gpx &gpx_wnd::draw_text(const std::string &text, point p)
    {
        auto *cache = motif::wnd_gpx_bindings.from_a(_wnd);
        Widget canvas = motif::wnd_bindings.from_b(_wnd);
        if (!cache || !cache->backbuffer || !canvas)
            return *this;

        apply_gc(canvas, this, cache);

        Font font = XLoadFont(motif::cached_display, "-misc-fixed-medium-r-normal--13-120-75-75-c-70-iso8859-1");
        XSetFont(motif::cached_display, cache->gc, font);
        XDrawString(motif::cached_display, cache->backbuffer, cache->gc, p.x, p.y, text.c_str(), text.length());
        XUnloadFont(motif::cached_display, font);
        return *this;
    }

    gpx &gpx_wnd::draw_img(const img &src, point dst)
    {
        auto *cache = motif::wnd_gpx_bindings.from_a(_wnd);
        if (!cache || !cache->backbuffer)
            return *this;

        XImage *ximg = XCreateImage(
            motif::cached_display,
            DefaultVisual(motif::cached_display, DefaultScreen(motif::cached_display)),
            DefaultDepth(motif::cached_display, DefaultScreen(motif::cached_display)),
            ZPixmap,
            0,
            reinterpret_cast<char *>(const_cast<rgba *>(src.pixels())),
            src.w(),
            src.h(),
            32,
            0);

        XPutImage(
            motif::cached_display,
            cache->backbuffer,
            cache->gc,
            ximg,
            0, 0,
            dst.x, dst.y,
            src.w(), src.h());
        XDestroyImage(ximg);
        return *this;
    }

} // namespace native
