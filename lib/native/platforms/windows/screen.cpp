//
// Implements the Windows display-detection backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <windows.h>

#include <native.h>

namespace native
{
    // Add one Windows monitor to the process-owned screen snapshot.
    static BOOL CALLBACK monitor_enum_proc(
        HMONITOR monitor,
        HDC,
        LPRECT,
        LPARAM data) {
        auto *screens = reinterpret_cast<std::vector<screen> *>(data);
        MONITORINFOEX info = {};
        info.cbSize = sizeof(info);

        if (!GetMonitorInfo(monitor, &info)) {
            return FALSE;
        }

        const RECT r = info.rcMonitor;
        const RECT w = info.rcWork;

        rect bounds(r.left, r.top, r.right - r.left, r.bottom - r.top);
        rect work_area(
            w.left,
            w.top,
            w.right - w.left,
            w.bottom - w.top);
        bool is_primary = (info.dwFlags & MONITORINFOF_PRIMARY) != 0;

        int index = static_cast<int>(screens->size());
        screens->emplace_back(index, bounds, work_area, is_primary);
        return TRUE;
    }

    const std::vector<screen> &screen::detect() {
        _screens.clear();

        if (!EnumDisplayMonitors(
                nullptr,
                nullptr,
                monitor_enum_proc,
                reinterpret_cast<LPARAM>(&_screens))) {
            _screens.clear();
            throw std::runtime_error(
                "Windows: Failed to enumerate monitors.");
        }

        normalize();
        return _screens;
    }
} // namespace native
