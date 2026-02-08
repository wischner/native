#include <native.h>
#include "gpx_img.h"

namespace native
{

    gpx &img::get_gpx() const
    {
        if (!_gpx)
        {
            _gpx = std::make_unique<gpx_img>(*this);
        }
        return *_gpx;
    }

} // namespace native
