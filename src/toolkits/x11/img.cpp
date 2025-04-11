// src/img.cpp
#include <native.h>

namespace native
{
    gpx &img::get_gpx() const
    {
        if (!_gpx)
        {
            _gpx = x11::create_gpx_img(const_cast<img *>(this));
        }
        return *_gpx;
    }

} // namespace native