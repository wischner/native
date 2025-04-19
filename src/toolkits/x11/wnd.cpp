#include <Xm/Xm.h>

#include <native.h>
#include <bindings.h>

#include <gpx_wnd.h>

namespace native
{
    gpx &wnd::get_gpx() const
    {
        if (!_gpx)
        {
            _gpx = new native::gpx_wnd((const_cast<wnd *>(this)));
        }
        return *_gpx;
    }
}