#include <native.h>

namespace native
{

    pen::pen(rgba color, coord thickness)
        : _color(color), _thickness(thickness)
    {
    }

    pen::~pen() = default;

    pen::pen(pen &&other) noexcept
        : _color(other._color), _thickness(other._thickness)
    {
    }

    pen &pen::operator=(pen &&other) noexcept
    {
        if (this != &other)
        {
            _color = other._color;
            _thickness = other._thickness;
        }
        return *this;
    }

    rgba pen::color() const
    {
        return _color;
    }

    coord pen::thickness() const
    {
        return _thickness;
    }

}
