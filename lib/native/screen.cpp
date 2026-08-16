//
// Implements backend-neutral display collection queries.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <iterator>

#include <native/screen.h>

namespace native
{
    std::vector<screen> screen::_screens;

    screen::screen(int index,
                   const rect &bounds,
                   const rect &work_area,
                   bool is_primary)
        : _index(index)
        , _bounds(bounds)
        , _work_area(work_area.intersect(bounds))
        , _is_primary(is_primary) {
        if (_work_area.w() == 0 || _work_area.h() == 0)
            _work_area = _bounds;
    }

    int screen::index() const {
        return _index;
    }
    bool screen::is_primary() const {
        return _is_primary;
    }
    bool screen::is_landscape() const {
        return _bounds.w() >= _bounds.h();
    }
    const rect &screen::bounds() const {
        return _bounds;
    }
    const rect &screen::work_area() const {
        return _work_area;
    }

    int screen::count() {
        return static_cast<int>(_screens.size());
    }

    screen *screen::at(int index) {
        if (index < 0 || index >= static_cast<int>(_screens.size()))
            return nullptr;
        return &_screens[index];
    }

    screen *screen::primary() {
        const auto primary =
            std::find_if(_screens.begin(),
                         _screens.end(),
                         [](const screen &candidate) {
                             return candidate._is_primary;
                         });
        return primary == _screens.end() ? nullptr : &*primary;
    }

    rect screen::virtual_bounds() {
        if (_screens.empty())
            return {};

        rect bounds = _screens.front().bounds();
        for (auto current = std::next(_screens.begin());
             current != _screens.end();
             ++current) {
            const screen &s = *current;
            int x1 = std::min(bounds.x1(), s.bounds().x1());
            int y1 = std::min(bounds.y1(), s.bounds().y1());
            int x2 = std::max(bounds.x2(), s.bounds().x2());
            int y2 = std::max(bounds.y2(), s.bounds().y2());

            bounds = rect(x1, y1, x2 - x1, y2 - y1);
        }

        return bounds;
    }

    void screen::normalize() {
        bool found_primary = false;

        for (std::size_t index = 0; index < _screens.size(); ++index) {
            screen &current = _screens[index];
            current._index = static_cast<int>(index);

            if (current._is_primary && !found_primary) {
                found_primary = true;
            } else {
                current._is_primary = false;
            }
        }

        if (!found_primary && !_screens.empty())
            _screens.front()._is_primary = true;
    }

} // namespace native
