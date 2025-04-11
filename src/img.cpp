#include <native.h>

namespace native
{
    img::img(coord w, coord h) : _w(w), _h(h)
    {
        _data = new rgba[w * h]();
    }

    img::~img()
    {
        delete[] _data;
        delete _gpx;
    }
}
