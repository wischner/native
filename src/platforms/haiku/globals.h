#pragma once

#include <set>

#include <Application.h>
#include <Window.h>
#include <View.h>
#include <native.h>
#include <bindings.h>

namespace haiku
{
    // Platform handle for a font_t — copies a BFont value.
    struct haikufont
    {
        BFont bfont;
    };

    // Graphics cache structure for Haiku BeAPI
    typedef struct
    {
        BView *view = nullptr; // Cached BView for drawing

        // Cached draw parameters
        native::rgba current_fg = 0xFFFFFFFF;
        int current_thickness = -1;

        // Clip region
        native::rect clip = {};
        bool dirty_clip = true;
    } haikugpx;

    struct haikumenu {
        native::app_wnd *owner = nullptr;
        std::set<int> item_ids;
    };

    extern BApplication *global_app;
    extern native::bindings<BWindow *, native::wnd *> wnd_bindings;
    extern native::bindings<native::wnd *, haikugpx *> wnd_gpx_bindings;
    extern native::bindings<uint32_t, haikufont *> font_bindings;
    extern native::bindings<uint32_t, haikumenu *> menu_bindings;
    extern native::bindings<native::app_wnd *, haikumenu *> owner_menu_bindings;
}
