//
// Declares use of the active native control font in composite children.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <X11/Intrinsic.h>
namespace linux::openmotif
{
    // Apply the font selected by native push buttons without changing
    // CDE resources.
    void apply_control_font(Widget widget);
} // namespace linux::openmotif
