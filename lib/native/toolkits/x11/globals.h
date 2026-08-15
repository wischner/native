//
// Declares internal X11 shared backend-state types and state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>
#include <vector>
#include <utility>

#include <X11/Xlib.h>

#include <native.h>
#include <bindings.h>

namespace linux::x11
{
    // Xlib callbacks carry handles, so process-wide registries recover
    // the corresponding C++ objects during event dispatch.
    extern native::bindings<Window, native::wnd *> wnd_bindings;
    extern Display *cached_display;
    extern Atom wm_delete_window_atom;

    // Platform handle for a font_t — owns an X11 core font.
    struct x11_font
    {
        Display *display;
        Font xfont;
        bool owned;  // if true, XUnloadFont on destruction
    };

    // Internally cached values for gc and backbuffer
    struct x11_gpx
    {
        GC gc = nullptr;

        // Off-screen backbuffer (eliminates white-flash on repaint)
        Pixmap backbuffer = 0;
        int buf_w = 0;
        int buf_h = 0;

        // Cached draw parameters
        native::rgba current_fg = 0xFFFFFFFF;
        int current_thickness = -1;
    };

    extern native::bindings<native::wnd *, x11_gpx *> wnd_gpx_bindings;
    extern native::bindings<uint32_t, x11_font *> font_bindings;

    static constexpr int menu_bar_height = 20;

    struct x11_menu {
        Window bar_win   = 0;
        Window popup_win = 0;
        GC     gc        = nullptr;
        native::app_wnd *owner = nullptr;
        unsigned long bar_bg = 0xD4D0C8UL;
        unsigned long border_light = 0;
        unsigned long border_dark = 0;
        unsigned long text_fg = 0;
        unsigned long select_bg = 0x000080UL;
        unsigned long select_fg = 0;
        struct top_entry {
            std::string title;
            std::vector<std::pair<int, std::string>> items;
            int x0 = 0, x1 = 0;
        };
        std::vector<top_entry> tops;
        int open_idx = -1;
        int hover_top = -1;
        int hover_item = -1;
    };

    // Translate an X event targeting an emulated menu bar or popup.
    void handle_menu_bar_event(x11_menu *menu, const XEvent &event);

    extern native::bindings<Window,   x11_menu *> menu_bar_bindings;
    extern native::bindings<uint32_t, x11_menu *> menu_bindings;

    struct x11_button
    {
        Window win = 0;
        GC gc = nullptr;
        native::button *owner = nullptr;
        bool hover = false;
        bool pressed = false;
    };

    extern native::bindings<
        native::button *,
        x11_button *> button_bindings;
    // Translate an X event targeting an emulated button.
    void handle_button_event(
        native::button *button,
        const XEvent &event);
}
