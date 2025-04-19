#include <stdexcept>
#include <algorithm>

#include <X11/Xutil.h>
#include <X11/Xlib.h>

#include <native.h>
#include "gpx_img.h"
#include "globals.h"

namespace x11
{
    extern Display *cached_display;

    native::gpx *create_gpx_img(native::img *image)
    {
        return new native::gpx_img(image);
    }
}

namespace native
{

    gpx_img::gpx_img(const img *image)
        : _img(image), _pen(rgba(0, 0, 0, 255), 1)
    {
        if (!x11::cached_display)
        {
            throw std::runtime_error("X11: No display available for gpx_img");
        }
    }

    void gpx_img::set_pen(const pen &p)
    {
        _pen = p;
    }

    pen gpx_img::get_pen() const
    {
        return _pen;
    }

    void gpx_img::clear(rgba color)
    {
        rgba *pixels = const_cast<rgba *>(_img->pixels());
        int x1 = std::max(0, _clip.p.x);
        int y1 = std::max(0, _clip.p.y);
        int x2 = std::min(_img->width() - 1, _clip.x2());
        int y2 = std::min(_img->height() - 1, _clip.y2());

        for (int y = y1; y <= y2; ++y)
        {
            for (int x = x1; x <= x2; ++x)
            {
                pixels[y * _img->width() + x] = color;
            }
        }
    }

    void gpx_img::draw_line(point from, point to)
    {
        int x0 = from.x, y0 = from.y, x1 = to.x, y1 = to.y;
        int dx = abs(x1 - x0), dy = abs(y1 - y0);
        int sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1;
        int err = dx - dy;

        rgba *pixels = const_cast<rgba *>(_img->pixels());
        int clip_x1 = _clip.p.x, clip_y1 = _clip.p.y;
        int clip_x2 = _clip.x2(), clip_y2 = _clip.y2();

        while (true)
        {
            if (x0 >= clip_x1 && x0 <= clip_x2 && y0 >= clip_y1 && y0 <= clip_y2 &&
                x0 >= 0 && x0 < _img->width() && y0 >= 0 && y0 < _img->height())
            {
                pixels[y0 * _img->width() + x0] = _pen.color;
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
    }

    void gpx_img::draw_rect(rect r, bool filled)
    {
        int x1 = std::max(std::max(0, r.p.x), _clip.p.x);
        int y1 = std::max(std::max(0, r.p.y), _clip.p.y);
        int x2 = std::min(std::min(_img->width() - 1, r.x2()), _clip.x2());
        int y2 = std::min(std::min(_img->height() - 1, r.y2()), _clip.y2());

        if (x1 > x2 || y1 > y2)
            return;

        rgba *pixels = const_cast<rgba *>(_img->pixels());
        if (filled)
        {
            for (int y = y1; y <= y2; ++y)
            {
                for (int x = x1; x <= x2; ++x)
                {
                    pixels[y * _img->width() + x] = _pen.color;
                }
            }
        }
        else
        {
            for (int x = x1; x <= x2; ++x)
            {
                pixels[y1 * _img->width() + x] = _pen.color;
                pixels[y2 * _img->width() + x] = _pen.color;
            }
            for (int y = y1 + 1; y < y2; ++y)
            {
                pixels[y * _img->width() + x1] = _pen.color;
                pixels[y * _img->width() + x2] = _pen.color;
            }
        }
    }

    void gpx_img::draw_text(const std::string &text, point p)
    {
        Display *display = x11::cached_display;
        XImage *ximg = XCreateImage(display, DefaultVisual(display, DefaultScreen(display)),
                                    DefaultDepth(display, DefaultScreen(display)), ZPixmap, 0,
                                    reinterpret_cast<char *>(const_cast<rgba *>(_img->pixels())),
                                    _img->width(), _img->height(), 32, 0);
        GC gc = XCreateGC(display, DefaultRootWindow(display), 0, nullptr);
        XSetForeground(display, gc, _pen.color);
        Font font = XLoadFont(display, "-misc-fixed-medium-r-normal--13-120-75-75-c-70-iso8859-1");
        XSetFont(display, gc, font);

        // Apply clipping to the GC
        XRectangle xr = {static_cast<short>(_clip.p.x), static_cast<short>(_clip.p.y),
                         static_cast<unsigned short>(_clip.d.w), static_cast<unsigned short>(_clip.d.h)};
        XSetClipRectangles(display, gc, 0, 0, &xr, 1, Unsorted);

        XDrawString(display, reinterpret_cast<Drawable>(ximg->data), gc, p.x, p.y, text.c_str(), text.length());
        XUnloadFont(display, font);
        XFreeGC(display, gc);
        XDestroyImage(ximg);
    }

    void gpx_img::draw_img(const img &src, point dst)
    {
        rgba *dst_pixels = const_cast<rgba *>(_img->pixels());
        const rgba *src_pixels = src.pixels();
        int clip_x1 = _clip.p.x, clip_y1 = _clip.p.y;
        int clip_x2 = _clip.x2(), clip_y2 = _clip.y2();

        for (coord y = 0; y < src.height() && (dst.y + y) < _img->height(); ++y)
        {
            for (coord x = 0; x < src.width() && (dst.x + x) < _img->width(); ++x)
            {
                int dst_x = dst.x + x, dst_y = dst.y + y;
                if (dst_x >= clip_x1 && dst_x <= clip_x2 && dst_y >= clip_y1 && dst_y <= clip_y2)
                {
                    dst_pixels[dst_y * _img->width() + dst_x] = src_pixels[y * src.width() + x];
                }
            }
        }
    }

    void gpx_img::set_clip(const rect &r)
    {
        _clip = r;
    }

    rect gpx_img::get_clip() const
    {
        return _clip;
    }

} // namespace native