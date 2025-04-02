#include <native.h>

namespace native
{
    std::vector<native::screen> screens;

    screen::screen(int index, const rect &bounds, const rect &work_area, bool is_primary)
        : _index(index), _bounds(bounds), _work_area(work_area), _is_primary(is_primary) {}

    int screen::count()
    {
        return static_cast<int>(screens.size());
    }

    screen *screen::at(int index)
    {
        if (index < 0 || index >= static_cast<int>(screens.size()))
            return nullptr;
        return &screens[index];
    }

    screen *screen::primary()
    {
        for (auto &s : screens)
            if (s.is_primary())
                return &s;
        return nullptr;
    }

    rect screen::virtual_bounds()
    {
        if (screens.empty())
            return rect{};

        coord min_x = screens[0].bounds().x1();
        coord min_y = screens[0].bounds().y1();
        coord max_x = screens[0].bounds().x2();
        coord max_y = screens[0].bounds().y2();

        for (const auto &s : screens)
        {
            min_x = std::min(min_x, s.bounds().x1());
            min_y = std::min(min_y, s.bounds().y1());
            max_x = std::max(max_x, s.bounds().x2());
            max_y = std::max(max_y, s.bounds().y2());
        }

        return rect(min_x, min_y, max_x - min_x, max_y - min_y);
    }

    int screen::index() const { return _index; }
    bool screen::is_primary() const { return _is_primary; }
    const rect &screen::bounds() const { return _bounds; }
    const rect &screen::work_area() const { return _work_area; }

} // namespace native
