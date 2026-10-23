//
// Declares native Window Maker modal panel positioning.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <WINGs/WINGs.h>
namespace linux::wmaker
{
    // Translate a native widget origin into desktop coordinates.
    WMPoint screen_position(WMWidget *widget);
    // Center a panel immediately and after its native map transaction.
    void position_dialog(WMWindow *dialog, WMWindow *owner);
    // Detach the temporary map callback from a cached standard panel.
    void finish_position_dialog(WMWindow *dialog, WMWindow *owner);
} // namespace linux::wmaker
