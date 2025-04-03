#include <native.h>
#include <windows.h>
#include <vector>
#include <iostream>

namespace native {

extern  std::vector<screen> screens;

BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM)
{
    MONITORINFOEX info;
    info.cbSize = sizeof(info);

    if (!GetMonitorInfo(hMonitor, &info)) {
        std::cerr << "Windows: Failed to get monitor info.\n";
        return TRUE;
    }

    RECT r = info.rcMonitor;
    RECT w = info.rcWork;

    rect bounds(r.left, r.top, r.right - r.left, r.bottom - r.top);
    rect work_area(w.left, w.top, w.right - w.left, w.bottom - w.top);
    bool is_primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;

    int index = static_cast<int>(screens.size());
    screens.emplace_back(index, bounds, work_area, is_primary);

    return TRUE;
}

void screen::detect()
{
    screens.clear();

    if (!EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, 0)) {
        std::cerr << "Windows: Failed to enumerate monitors.\n";
    }
}

} // namespace native
