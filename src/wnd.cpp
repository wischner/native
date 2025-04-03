#include <native.h>

namespace native
{
    wnd::wnd(int x, int y, int width, int height)
        : _x(x), _y(y), _width(width), _height(height) {}

    int wnd::x() const { return _x; }
    int wnd::y() const { return _y; }
    int wnd::width() const { return _width; }
    int wnd::height() const { return _height; }
}