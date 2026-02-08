#pragma once

#include <Application.h>
#include <Window.h>
#include <View.h>
#include <native.h>
#include <bindings.h>

namespace haiku
{
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

    extern BApplication *global_app;
    extern native::bindings<BWindow *, native::wnd *> wnd_bindings;
    extern native::bindings<native::wnd *, haikugpx *> wnd_gpx_bindings;
}
