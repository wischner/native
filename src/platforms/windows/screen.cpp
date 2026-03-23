#include <iostream>

#include <windows.h>

#include <native.h>

namespace native
{
    static BOOL CALLBACK monitor_enum_proc(HMONITOR monitor, HDC, LPRECT, LPARAM data)
    {
        auto *screens = reinterpret_cast<std::vector<screen> *>(data);
        MONITORINFOEX info = {};
        info.cbSize = sizeof(info);

        if (!GetMonitorInfo(monitor, &info))
        {
            std::cerr << "Windows: Failed to get monitor info." << std::endl;
            return TRUE;
        }

        const RECT r = info.rcMonitor;
        const RECT w = info.rcWork;

        rect bounds(r.left, r.top, r.right - r.left, r.bottom - r.top);
        rect work_area(w.left, w.top, w.right - w.left, w.bottom - w.top);
        bool is_primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;

        int index = static_cast<int>(screens->size());
        screens->emplace_back(index, bounds, work_area, is_primary);
        return TRUE;
    }

    const std::vector<screen> &screen::detect()
    {
        _screens.clear();

        if (!EnumDisplayMonitors(nullptr, nullptr, monitor_enum_proc, reinterpret_cast<LPARAM>(&_screens)))
            std::cerr << "Windows: Failed to enumerate monitors." << std::endl;

        return _screens;
    }
} // namespace native
