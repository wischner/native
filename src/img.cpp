#include <stdexcept>

#include <native.h>

namespace native
{
    img::img(dim w, dim h)
        : _w(w), _h(h), _data(std::make_unique<rgba[]>(_w * _h))
    {
        if (w == 0 || h == 0)
            throw std::invalid_argument("img: dimensions must be > 0");
    }

    img::~img() = default;

} // namespace native