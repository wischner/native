#pragma once

#include <windows.h>

#include <native.h>
#include <bindings.h>

namespace win
{
    // Platform handle for a font_t — owns the GDI HFONT.
    struct winfont
    {
        HFONT hfont = nullptr;
    };

    // Graphics cache structure for Windows GDI
    typedef struct
    {
        HDC hdc = nullptr;         // Cached device context
        HPEN pen = nullptr;        // Cached pen
        HBRUSH brush = nullptr;    // Cached brush

        // Cached draw parameters
        native::rgba current_fg = 0xFFFFFFFF;
        int current_thickness = -1;

        // Clip region
        native::rect clip = {};
        bool dirty_clip = true;
    } wingpx;

    struct winmenu {
        HMENU hmenu = nullptr;
        native::app_wnd *owner = nullptr;
    };

    extern native::bindings<HWND, native::wnd *> wnd_bindings;
    extern native::bindings<native::wnd *, wingpx *> wnd_gpx_bindings;
    extern native::bindings<uint32_t, winfont *> font_bindings;
    extern native::bindings<uint32_t, winmenu *> menu_bindings;
}
