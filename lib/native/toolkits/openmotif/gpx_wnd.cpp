//
// Implements the OpenMotif window-graphics backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <Xm/Xm.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <native.h>

#include "gpx_wnd.h"
#include "globals.h"
#include "../../platforms/linux/x_image.h"

namespace
{
    Colormap widget_colormap(Widget widget) {
        Colormap colormap = DefaultColormapOfScreen(XtScreen(widget));
        XtVaGetValues(widget, XtNcolormap, &colormap, nullptr);
        return colormap;
    }

    Pixel rgba_to_pixel(Widget widget, native::rgba color) {
        if (!widget || !linux::openmotif::cached_display)
            return 0;

        XColor xc = {};
        xc.red = static_cast<unsigned short>(color.r) * 257;
        xc.green = static_cast<unsigned short>(color.g) * 257;
        xc.blue = static_cast<unsigned short>(color.b) * 257;

        Colormap colormap = widget_colormap(widget);
        if (XAllocColor(
                linux::openmotif::cached_display, colormap, &xc))
            return xc.pixel;

        return BlackPixelOfScreen(XtScreen(widget));
    }

    native::rgba pixel_to_rgba(Widget widget, Pixel pixel) {
        if (!widget || !linux::openmotif::cached_display)
            return native::rgba(0, 0, 0, 255);

        XColor xc = {};
        xc.pixel = pixel;
        XQueryColor(linux::openmotif::cached_display,
                    widget_colormap(widget),
                    &xc);
        return native::rgba(static_cast<uint8_t>(xc.red >> 8),
                            static_cast<uint8_t>(xc.green >> 8),
                            static_cast<uint8_t>(xc.blue >> 8),
                            255);
    }

    void apply_gc(Widget widget,
                  native::gpx_wnd *self,
                  linux::openmotif::motif_gpx *cache) {
        if (!widget || !cache || !cache->gc)
            return;

        if (cache->current_fg != self->get_ink()) {
            XSetForeground(linux::openmotif::cached_display,
                           cache->gc,
                           rgba_to_pixel(widget, self->get_ink()));
            cache->current_fg = self->get_ink();
        }

        if (cache->current_thickness != self->get_pen()) {
            XSetLineAttributes(linux::openmotif::cached_display,
                               cache->gc,
                               self->get_pen(),
                               LineSolid,
                               CapButt,
                               JoinMiter);
            cache->current_thickness = self->get_pen();
        }

        const native::rect clip = self->get_clip();
        XRectangle xclip = {static_cast<short>(clip.p.x),
                            static_cast<short>(clip.p.y),
                            static_cast<unsigned short>(clip.d.w),
                            static_cast<unsigned short>(clip.d.h)};
        XSetClipRectangles(linux::openmotif::cached_display,
                           cache->gc,
                           0,
                           0,
                           &xclip,
                           1,
                           Unsorted);
    }
} // namespace

namespace native
{

    gpx_wnd::gpx_wnd(const wnd *window, point offset)
        : _wnd(const_cast<wnd *>(window))
        , _offset(offset) {
        if (!linux::openmotif::cached_display)
            throw std::runtime_error(
                "Motif: No display available for gpx_wnd.");

        Widget canvas =
            linux::openmotif::wnd_bindings.handle_from_object(_wnd);
        if (!canvas)
            throw std::runtime_error(
                "Motif: Drawing widget is missing.");

        auto *cache =
            linux::openmotif::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache) {
            cache = new linux::openmotif::motif_gpx();
            const Drawable drawable = XtIsRealized(canvas)
                ? XtWindow(canvas) : RootWindowOfScreen(XtScreen(canvas));
            cache->gc = XCreateGC(linux::openmotif::cached_display,
                                  drawable,
                                  0,
                                  nullptr);

            Dimension width = 1, height = 1;
            int depth = DefaultDepthOfScreen(XtScreen(canvas));
            XtVaGetValues(canvas, XmNwidth, &width, XmNheight, &height,
                          XmNdepth, &depth, nullptr);
            cache->backbuffer =
                XCreatePixmap(linux::openmotif::cached_display,
                              drawable,
                              std::max<unsigned int>(1, width),
                              std::max<unsigned int>(1, height),
                              static_cast<unsigned int>(depth));
            cache->buf_w = std::max<int>(1, width);
            cache->buf_h = std::max<int>(1, height);

            Pixel background = WhitePixelOfScreen(XtScreen(canvas));
            Pixel foreground = BlackPixelOfScreen(XtScreen(canvas));
            XtVaGetValues(canvas,
                          XmNbackground,
                          &background,
                          XmNforeground,
                          &foreground,
                          nullptr);

            set_paper(pixel_to_rgba(canvas, background));
            set_ink(pixel_to_rgba(canvas, foreground));

            XSetForeground(linux::openmotif::cached_display,
                           cache->gc,
                           background);
            XFillRectangle(linux::openmotif::cached_display,
                           cache->backbuffer,
                           cache->gc,
                           0,
                           0,
                           static_cast<unsigned int>(cache->buf_w),
                           static_cast<unsigned int>(cache->buf_h));
            cache->current_fg = get_paper();

            linux::openmotif::wnd_gpx_bindings.register_pair(_wnd,
                                                             cache);
        }
        const size dimensions = window->get_dimensions();
        _clip = rect(0, 0, dimensions.w, dimensions.h);
    }

    gpx_wnd::~gpx_wnd() {
        auto *cache =
            linux::openmotif::wnd_gpx_bindings.object_from_handle(_wnd);
        if (!cache)
            return;

        if (cache->gc && linux::openmotif::cached_display)
            XFreeGC(linux::openmotif::cached_display, cache->gc);
        if (cache->backbuffer && linux::openmotif::cached_display) {
            XFreePixmap(linux::openmotif::cached_display,
                        cache->backbuffer);
        }
        delete cache;
        linux::openmotif::wnd_gpx_bindings.unregister_by_handle(_wnd);
    }

    gpx &gpx_wnd::set_clip(const rect &r) {
        _clip = r;
        return *this;
    }

    rect gpx_wnd::get_clip() const {
        return _clip;
    }

    gpx &gpx_wnd::clear(rgba color) {
        auto *cache =
            linux::openmotif::wnd_gpx_bindings.object_from_handle(_wnd);
        Widget canvas =
            linux::openmotif::wnd_bindings.handle_from_object(_wnd);
        if (!cache || !cache->backbuffer || !canvas)
            return *this;

        XSetForeground(linux::openmotif::cached_display,
                       cache->gc,
                       rgba_to_pixel(canvas, color));
        XSetClipMask(linux::openmotif::cached_display,
                     cache->gc,
                     None);
        XFillRectangle(linux::openmotif::cached_display,
                       cache->backbuffer,
                       cache->gc,
                       _clip.p.x,
                       _clip.p.y,
                       static_cast<unsigned int>(_clip.d.w),
                       static_cast<unsigned int>(_clip.d.h));
        cache->current_fg = color;
        return *this;
    }

    gpx &gpx_wnd::draw_line(point from, point to) {
        auto *cache =
            linux::openmotif::wnd_gpx_bindings.object_from_handle(_wnd);
        Widget canvas =
            linux::openmotif::wnd_bindings.handle_from_object(_wnd);
        if (!cache || !cache->backbuffer || !canvas)
            return *this;

        apply_gc(canvas, this, cache);
        XDrawLine(linux::openmotif::cached_display,
                  cache->backbuffer,
                  cache->gc,
                  from.x,
                  from.y,
                  to.x,
                  to.y);
        return *this;
    }

    gpx &gpx_wnd::draw_rect(rect r, bool filled) {
        auto *cache =
            linux::openmotif::wnd_gpx_bindings.object_from_handle(_wnd);
        Widget canvas =
            linux::openmotif::wnd_bindings.handle_from_object(_wnd);
        if (!cache || !cache->backbuffer || !canvas)
            return *this;

        apply_gc(canvas, this, cache);

        if (filled) {
            XFillRectangle(linux::openmotif::cached_display,
                           cache->backbuffer,
                           cache->gc,
                           r.p.x,
                           r.p.y,
                           r.d.w,
                           r.d.h);
        } else {
            XDrawRectangle(linux::openmotif::cached_display,
                           cache->backbuffer,
                           cache->gc,
                           r.p.x,
                           r.p.y,
                           r.d.w - 1,
                           r.d.h - 1);
        }

        return *this;
    }

    gpx &gpx_wnd::draw_native_text(const std::string &text, point p) {
        if (_font && !_font->valid())
            return *this;
        auto *cache =
            linux::openmotif::wnd_gpx_bindings.object_from_handle(_wnd);
        Widget canvas =
            linux::openmotif::wnd_bindings.handle_from_object(_wnd);
        if (!cache || !cache->backbuffer || !canvas)
            return *this;

        apply_gc(canvas, this, cache);

        auto *fh = linux::openmotif::font_bindings.object_from_handle(
            get_font().id());
        if (fh && fh->xfont)
            XSetFont(
                linux::openmotif::cached_display, cache->gc, fh->xfont);

        const int baseline = p.y + get_font_metrics().ascent;
        XDrawString(linux::openmotif::cached_display,
                    cache->backbuffer,
                    cache->gc,
                    p.x,
                    baseline,
                    text.c_str(),
                    text.length());
        return *this;
    }

    gpx &gpx_wnd::draw_img(const img &src, point dst) {
        auto *cache =
            linux::openmotif::wnd_gpx_bindings.object_from_handle(_wnd);
        Widget canvas =
            linux::openmotif::wnd_bindings.handle_from_object(_wnd);
        if (!cache || !cache->backbuffer || !canvas)
            return *this;

        apply_gc(canvas, this, cache);
        detail::blend_x_image(linux::openmotif::cached_display,
                              cache->backbuffer,
                              cache->gc,
                              src,
                              dst,
                              _clip,
                              size(cache->buf_w, cache->buf_h));
        return *this;
    }

} // namespace native
