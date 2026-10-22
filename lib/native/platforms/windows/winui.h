//
// Ends the UI thread's WinUI hosting session after all native islands
// close. Windows App SDK types remain private to the settings adapter.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <windows.h>
namespace windows
{
    // Let WinUI consume island input before ordinary Win32 dispatch.
    bool pretranslate_winui(const MSG &message);
    // Release XAML, its dispatcher queue and runtime bootstrap in that
    // order.
    void shutdown_winui() noexcept;
} // namespace windows
