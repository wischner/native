//
// Implements the GEMix display-detection backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <native.h>
#include <native/screen.h>

#include "globals.h"

namespace native
{
    const std::vector<screen> &screen::detect() {
        _screens.clear();

        if (!linux::gemix::ensure_runtime())
            throw std::runtime_error(
                "GEMix: Failed to initialize the display runtime.");

        rect bounds = linux::gemix::screen_rect();
        rect work_area = linux::gemix::desktop_rect();
        if (bounds.w() == 0 || bounds.h() == 0 || work_area.w() == 0 ||
            work_area.h() == 0)
            throw std::runtime_error(
                "GEMix: Failed to query display geometry.");

        _screens.emplace_back(0, bounds, work_area, true);
        normalize();
        return _screens;
    }
} // namespace native
