#pragma once

#include <windows.h>

#include <native.h>
#include <bindings.h>

namespace win
{
    // Graphics cache structure for Windows GDI
    typedef struct
    {
        HDC hdc = nullptr;         // Cached device context
        HPEN pen = nullptr;        // Cached pen
        HBRUSH brush = nullptr;    // Cached brush
        HFONT font = nullptr;      // Cached font

        // Cached draw parameters
        native::rgba current_fg = 0xFFFFFFFF;
        int current_thickness = -1;

        // Clip region
        native::rect clip = {};
        bool dirty_clip = true;
    } wingpx;

    extern native::bindings<HWND, native::wnd *> wnd_bindings;
    extern native::bindings<native::wnd *, wingpx *> wnd_gpx_bindings;
}
