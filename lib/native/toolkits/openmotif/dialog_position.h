//
// Declares centering for Motif dialogs after preferred geometry is
// known.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <X11/Intrinsic.h>
namespace linux::openmotif
{
    // Disable Motif's screen placement and center on the owning widget.
    void position_dialog(Widget dialog, Widget owner);
} // namespace linux::openmotif
