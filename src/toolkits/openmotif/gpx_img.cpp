#include <stdexcept>
#include <algorithm>

#include <Xm/Xm.h>
#include <X11/Xutil.h>
#include <X11/Xlib.h>

#include <native.h>
#include "gpx_img.h"
#include "globals.h"

namespace native
{

    gpx_img::gpx_img(const img &image)
        : _img(image)
    {
        if (!motif::cached_display)
        {
            throw std::runtime_error("Motif: No display available for gpx_img");
        }
    }

    gpx &gpx_img::set_clip(const rect &r)
    {
        _clip = r;
        return *this;
    }

    rect gpx_img::clip() const
    {
        return _clip;
    }

    gpx &gpx_img::clear(rgba color)
    {
        rgba *pixels = const_cast<rgba *>(_img.pixels());
        int x1 = std::max(0, static_cast<int>(_clip.p.x));
        int y1 = std::max(0, static_cast<int>(_clip.p.y));
        int x2 = std::min(static_cast<int>(_img.w() - 1), static_cast<int>(_clip.x2()));
        int y2 = std::min(static_cast<int>(_img.h() - 1), static_cast<int>(_clip.y2()));

        for (int y = y1; y <= y2; ++y)
            for (int x = x1; x <= x2; ++x)
                pixels[y * _img.w() + x] = color;
        return *this;
    }

    gpx &gpx_img::draw_line(point from, point to)
    {
        int x0 = from.x, y0 = from.y, x1 = to.x, y1 = to.y;
        int dx = abs(x1 - x0), dy = abs(y1 - y0);
        int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
        int err = dx - dy;

        rgba *pixels = const_cast<rgba *>(_img.pixels());
        int clip_x1 = _clip.p.x, clip_y1 = _clip.p.y;
        int clip_x2 = _clip.x2(), clip_y2 = _clip.y2();

        while (true)
        {
            if (x0 >= clip_x1 && x0 <= clip_x2 && y0 >= clip_y1 && y0 <= clip_y2 &&
                x0 >= 0 && x0 < _img.w() && y0 >= 0 && y0 < _img.h())
            {
                pixels[y0 * _img.w() + x0] = _ink;
            }
            if (x0 == x1 && y0 == y1)
                break;
            int e2 = 2 * err;
            if (e2 > -dy)
            {
                err -= dy;
                x0 += sx;
            }
            if (e2 < dx)
            {
                err += dx;
                y0 += sy;
            }
        }
        return *this;
    }

    gpx &gpx_img::draw_rect(rect r, bool filled)
    {
        int x1 = std::max(0, std::max(static_cast<int>(r.p.x), static_cast<int>(_clip.p.x)));
        int y1 = std::max(0, std::max(static_cast<int>(r.p.y), static_cast<int>(_clip.p.y)));
        int x2 = std::min(static_cast<int>(_img.w() - 1), std::min(static_cast<int>(r.x2()), static_cast<int>(_clip.x2())));
        int y2 = std::min(static_cast<int>(_img.h() - 1), std::min(static_cast<int>(r.y2()), static_cast<int>(_clip.y2())));

        if (x1 > x2 || y1 > y2)
            return *this;

        rgba *pixels = const_cast<rgba *>(_img.pixels());

        if (filled)
        {
            for (int y = y1; y <= y2; ++y)
                for (int x = x1; x <= x2; ++x)
                    pixels[y * _img.w() + x] = _ink;
        }
        else
        {
            for (int x = x1; x <= x2; ++x)
            {
                pixels[y1 * _img.w() + x] = _ink;
                pixels[y2 * _img.w() + x] = _ink;
            }
            for (int y = y1 + 1; y < y2; ++y)
            {
                pixels[y * _img.w() + x1] = _ink;
                pixels[y * _img.w() + x2] = _ink;
            }
        }
        return *this;
    }

    gpx &gpx_img::draw_text(const std::string &text, point p)
    {
        Display *display = motif::cached_display;
        XImage *ximg = XCreateImage(display, DefaultVisual(display, DefaultScreen(display)),
                                    DefaultDepth(display, DefaultScreen(display)), ZPixmap, 0,
                                    reinterpret_cast<char *>(const_cast<rgba *>(_img.pixels())),
                                    _img.w(), _img.h(), 32, 0);

        GC gc = XCreateGC(display, DefaultRootWindow(display), 0, nullptr);
        XSetForeground(display, gc, _ink);
        Font font = XLoadFont(display, "-misc-fixed-medium-r-normal--13-120-75-75-c-70-iso8859-1");
        XSetFont(display, gc, font);

        XRectangle xr = {static_cast<short>(_clip.p.x), static_cast<short>(_clip.p.y),
                         static_cast<unsigned short>(_clip.d.w), static_cast<unsigned short>(_clip.d.h)};
        XSetClipRectangles(display, gc, 0, 0, &xr, 1, Unsorted);

        XDrawString(display, reinterpret_cast<Drawable>(ximg->data), gc, p.x, p.y, text.c_str(), text.length());

        XUnloadFont(display, font);
        XFreeGC(display, gc);
        XDestroyImage(ximg);
        return *this;
    }

    gpx &gpx_img::draw_img(const img &src, point dst)
    {
        rgba *dst_pixels = const_cast<rgba *>(_img.pixels());
        const rgba *src_pixels = src.pixels();

        int clip_x1 = _clip.p.x, clip_y1 = _clip.p.y;
        int clip_x2 = _clip.x2(), clip_y2 = _clip.y2();

        for (coord y = 0; y < src.h() && (dst.y + y) < _img.h(); ++y)
        {
            for (coord x = 0; x < src.w() && (dst.x + x) < _img.w(); ++x)
            {
                int dst_x = dst.x + x, dst_y = dst.y + y;
                if (dst_x >= clip_x1 && dst_x <= clip_x2 && dst_y >= clip_y1 && dst_y <= clip_y2)
                {
                    dst_pixels[dst_y * _img.w() + dst_x] = src_pixels[y * src.w() + x];
                }
            }
        }
        return *this;
    }

} // namespace native
