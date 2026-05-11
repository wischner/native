#include <algorithm>
#include <cstring>

#include <native.h>

#include "gpx_img.h"

namespace
{
    void put_pixel(native::img &img, int x, int y, native::rgba color)
    {
        if (x < 0 || y < 0 || x >= img.w() || y >= img.h())
            return;
        img.pixels()[y * img.w() + x] = color;
    }

    void draw_line_software(native::img &img,
                            native::point from,
                            native::point to,
                            native::rgba color)
    {
        int x0 = from.x;
        int y0 = from.y;
        int x1 = to.x;
        int y1 = to.y;
        int dx = std::abs(x1 - x0);
        int sx = x0 < x1 ? 1 : -1;
        int dy = -std::abs(y1 - y0);
        int sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;

        while (true)
        {
            put_pixel(img, x0, y0, color);
            if (x0 == x1 && y0 == y1)
                break;
            int e2 = 2 * err;
            if (e2 >= dy)
            {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx)
            {
                err += dx;
                y0 += sy;
            }
        }
    }
}

namespace native
{
    gpx_img::gpx_img(const img &image)
        : _img(image), _clip(0, 0, image.w(), image.h())
    {
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
        std::fill_n(const_cast<rgba *>(_img.pixels()), _img.w() * _img.h(), color);
        return *this;
    }

    gpx &gpx_img::draw_line(point from, point to)
    {
        draw_line_software(const_cast<img &>(_img), from, to, ink());
        return *this;
    }

    gpx &gpx_img::draw_rect(rect r, bool filled)
    {
        if (filled)
        {
            for (int y = r.p.y; y < r.p.y + r.d.h; ++y)
            {
                for (int x = r.p.x; x < r.p.x + r.d.w; ++x)
                    put_pixel(const_cast<img &>(_img), x, y, ink());
            }
            return *this;
        }

        draw_line(point(r.p.x, r.p.y), point(r.p.x + r.d.w - 1, r.p.y));
        draw_line(point(r.p.x, r.p.y), point(r.p.x, r.p.y + r.d.h - 1));
        draw_line(point(r.p.x + r.d.w - 1, r.p.y), point(r.p.x + r.d.w - 1, r.p.y + r.d.h - 1));
        draw_line(point(r.p.x, r.p.y + r.d.h - 1), point(r.p.x + r.d.w - 1, r.p.y + r.d.h - 1));
        return *this;
    }

    gpx &gpx_img::draw_text(const std::string &, point)
    {
        return *this;
    }

    gpx &gpx_img::draw_img(const img &src, point dst)
    {
        for (int y = 0; y < src.h(); ++y)
        {
            for (int x = 0; x < src.w(); ++x)
                put_pixel(const_cast<img &>(_img), dst.x + x, dst.y + y, src.pixels()[y * src.w() + x]);
        }
        return *this;
    }
}
