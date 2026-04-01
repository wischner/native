#pragma once

#include <string>
#include <vector>
#include <utility>

#include <X11/Xlib.h>

#include <native.h>
#include <bindings.h>

namespace x11
{
    extern native::bindings<Window, native::wnd *> wnd_bindings;
    extern Display *cached_display;
    extern Atom wm_delete_window_atom;

    // Platform handle for a font_t — owns an X11 core font.
    struct x11font
    {
        Display *display;
        Font xfont;
        bool owned;  // if true, XUnloadFont on destruction
    };

    // Internally cached values for gc and backbuffer
    typedef struct
    {
        GC gc = nullptr;

        // Off-screen backbuffer (eliminates white-flash on repaint)
        Pixmap backbuffer = 0;
        int buf_w = 0;
        int buf_h = 0;

        // Cached draw parameters
        native::rgba current_fg = 0xFFFFFFFF;
        int current_thickness = -1;
    } x11gpx;

    extern native::bindings<native::wnd *, x11gpx *> wnd_gpx_bindings;
    extern native::bindings<uint32_t, x11font *> font_bindings;

    static constexpr int MENU_BAR_H = 20;

    struct x11menu {
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

    void handle_menu_bar_event(x11menu *m, const XEvent &e);

    extern native::bindings<Window,   x11menu *> menubar_bindings;
    extern native::bindings<uint32_t, x11menu *> menu_bindings;

    struct x11button
    {
        Window win = 0;
        GC gc = nullptr;
        native::button *owner = nullptr;
        bool hover = false;
        bool pressed = false;
    };

    extern native::bindings<native::button *, x11button *> button_bindings;
    void handle_button_event(native::button *b, const XEvent &e);
}
