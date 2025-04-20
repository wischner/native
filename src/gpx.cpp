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

} // namespace native
