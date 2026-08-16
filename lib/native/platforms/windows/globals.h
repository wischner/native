//
// Declares internal Windows shared backend-state types and state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <windows.h>
#include <string>

#include <native.h>
#include <bindings.h>

namespace windows
{
    // Win32 callbacks carry handles rather than C++ object context, so
    // backend keeps process-wide registries for callback dispatch.
    // Platform handle for a font_t — owns the GDI HFONT.
    struct win_font
    {
        HFONT hfont = nullptr;
    };

    // Graphics cache structure for Windows GDI
    struct win_gpx
    {
        HDC hdc = nullptr;      // Cached device context
        HPEN pen = nullptr;     // Cached pen
        HBRUSH brush = nullptr; // Cached brush

        // Cached draw parameters
        native::rgba current_fg = 0xFFFFFFFF;
        int current_thickness = -1;

        // Clip region
        native::rect clip = {};
        bool dirty_clip = true;
    };

    struct win_menu
    {
        HMENU hmenu = nullptr;
        native::app_wnd *owner = nullptr;
    };

    struct win_button
    {
        HWND hwnd = nullptr;
        native::button *owner = nullptr;
    };

    // Stores the subclass state for one Win32 EDIT control.
    struct win_text_edit
    {
        HWND hwnd = nullptr;
        WNDPROC original_proc = nullptr;
        bool suppress = false;
    };

    extern native::bindings<HWND, native::wnd *> wnd_bindings;
    extern native::bindings<native::wnd *, win_gpx *> wnd_gpx_bindings;
    extern native::bindings<uint32_t, win_font *> font_bindings;
    extern native::bindings<uint32_t, win_menu *> menu_bindings;
    extern native::bindings<native::button *, win_button *>
        button_bindings;
    extern native::bindings<native::text_edit *, win_text_edit *>
        text_edit_bindings;

    // Validate and cache one EN_CHANGE notification.
    void handle_text_edit_change(native::text_edit *editor);

    // Route Win32 messages to the C++ window registered for a handle.
    LRESULT CALLBACK routed_wnd_proc(HWND hwnd,
                                     UINT message,
                                     WPARAM wparam,
                                     LPARAM lparam);

    // Convert a Win32 system color to the public RGBA representation.
    native::rgba rgba_from_sys_color(int idx);

    // Convert UTF-8 text to a Win32 wide string.
    std::wstring utf8_to_wide(const std::string &text);

    // Convert a Win32 wide string to UTF-8 text.
    std::string wide_to_utf8(const std::wstring &text);

    // Convert public rectangle coordinates to an inclusive Win32 RECT.
    RECT to_rect(const native::rect &r);

    // Resolve the native window handle behind a graphics context.
    HWND hwnd_from_gpx(native::gpx &g);

    // Return the same system-selected font used by native controls and
    // theme painters.
    HFONT control_font();
} // namespace windows
