#include <algorithm>

#include <native.h>

#include "gpx_img.h"

namespace native
{

gpx_img::gpx_img(const img &image)
    : _img(image)
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
    int x0 = from.x;
    int y0 = from.y;
    int x1 = to.x;
    int y1 = to.y;

    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;

    rgba *pixels = const_cast<rgba *>(_img.pixels());

    while (true)
    {
        if (x0 >= _clip.p.x && x0 <= _clip.x2() &&
            y0 >= _clip.p.y && y0 <= _clip.y2() &&
            x0 >= 0 && x0 < _img.w() &&
            y0 >= 0 && y0 < _img.h())
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
    (void)text;
    (void)p;
    return *this;
}

gpx &gpx_img::draw_img(const img &src, point dst)
{
    rgba *dst_pixels = const_cast<rgba *>(_img.pixels());
    const rgba *src_pixels = src.pixels();

    for (coord y = 0; y < src.h() && (dst.y + y) < _img.h(); ++y)
    {
        for (coord x = 0; x < src.w() && (dst.x + x) < _img.w(); ++x)
        {
            int dx = dst.x + x;
            int dy = dst.y + y;
            if (dx >= _clip.p.x && dx <= _clip.x2() &&
                dy >= _clip.p.y && dy <= _clip.y2())
            {
                dst_pixels[dy * _img.w() + dx] = src_pixels[y * src.w() + x];
            }
        }
    }

    return *this;
}

} // namespace native
