#include <native.h>

namespace native
{

    gpx &gpx::set_ink(rgba c)
    {
        _ink = c;
        return *this;
    }

    rgba gpx::ink() const
    {
        return _ink;
    }

    gpx &gpx::set_paper(rgba c)
    {
        _paper = c;
        return *this;
    }

    rgba gpx::paper() const
    {
        return _paper;
    }

    gpx &gpx::set_pen(uint8_t t)
    {
        _thickness = t;
        return *this;
    }

    uint8_t gpx::pen() const
    {
        return _thickness;
    }

    gpx &gpx::set_font(const font_t &f)
    {
        _font = &f;
        return *this;
    }

    const font_t &gpx::font() const
    {
        if (_font)
            return *_font;
        return font_t::stock(font_role::system);
    }

} // namespace native
