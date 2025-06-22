#include <native.h>
#include <algorithm>

namespace native
{

    std::vector<screen> screen::_screens;

    // Constructor
    screen::screen(int index, const rect &bounds, const rect &work_area, bool is_primary)
        : _index(index), _bounds(bounds), _work_area(work_area), _is_primary(is_primary)
    {
    }

    // Accessors
    int screen::index() const { return _index; }
    bool screen::is_primary() const { return _is_primary; }
    bool screen::is_landscape() const { return _bounds.w() > _bounds.h(); }
    const rect &screen::bounds() const { return _bounds; }
    const rect &screen::work_area() const { return _work_area; }

    // Static methods
    int screen::count()
    {
        return static_cast<int>(_screens.size());
    }

    screen *screen::at(int index)
    {
        if (index < 0 || index >= static_cast<int>(_screens.size()))
            return nullptr;
        return &_screens[index];
    }

    screen *screen::primary()
    {
        return _screens.empty() ? nullptr : &_screens[0];
    }

    rect screen::virtual_bounds()
    {
        if (_screens.empty())
            detect();

        rect bounds;
        for (const screen &s : _screens)
        {
            int x1 = std::min(bounds.x1(), s.bounds().x1());
            int y1 = std::min(bounds.y1(), s.bounds().y1());
            int x2 = std::max(bounds.x2(), s.bounds().x2());
            int y2 = std::max(bounds.y2(), s.bounds().y2());

            bounds = rect(x1, y1, x2 - x1, y2 - y1);
        }

        return bounds;
    }

} // namespace native
