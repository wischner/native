#include <native.h>
#include <gpx_img.h>

namespace native
{
    gpx &img::get_gpx() const
    {
        if (!_gpx)
        {
            _gpx = new native::gpx_img(const_cast<img *>(this));
        }
        return *_gpx;
    }

} // namespace native