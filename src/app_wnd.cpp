#include "native.h"

namespace native
{
    app_wnd::app_wnd(std::string title, int x, int y, int width, int height)
        : wnd(x, y, width, height), _title(std::move(title)) {}

    const std::string &app_wnd::title() const { return _title; }
}
