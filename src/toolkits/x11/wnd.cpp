#include <native.h>
#include <bindings.h>
#include <Xm/Xm.h>

namespace x11
{
    native::bindings<Widget, native::wnd *> wnd_bindings;
}

namespace native
{
    gpx &wnd::get_gpx() const
    {
        if (!_gpx)
        {
            _gpx = x11::create_gpx_wnd(const_cast<wnd *>(this));
        }
        return *_gpx;
    }
}