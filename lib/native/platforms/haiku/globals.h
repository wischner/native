//
// Declares internal Haiku shared backend-state types and state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <set>

#include <Application.h>
#include <Window.h>
#include <View.h>
#include <Button.h>
#include <native.h>
#include <bindings.h>

class BMenuBar;

namespace haiku
{
    // BeAPI callbacks carry objects, so process-wide registries
    // recover the corresponding C++ objects during event dispatch.
    // Platform handle for a font_t — copies a BFont value.
    struct haiku_font
    {
        BFont bfont;
    };

    // Graphics cache structure for Haiku BeAPI
    struct haiku_gpx
    {
        BView *view = nullptr; // Cached BView for drawing

        // Cached draw parameters
        native::rgba current_fg = 0xFFFFFFFF;
        int current_thickness = -1;

        // Clip region
        native::rect clip = {};
        bool dirty_clip = true;
    };

    struct haiku_menu {
        native::app_wnd *owner = nullptr;
        BMenuBar *bar = nullptr;
        std::set<int> item_ids;
    };

    struct haiku_button {
        BButton *button = nullptr;
        native::button *owner = nullptr;
    };

    extern BApplication *global_app;
    extern native::bindings<BWindow *, native::wnd *> wnd_bindings;
    extern native::bindings<
        native::wnd *,
        haiku_gpx *> wnd_gpx_bindings;
    extern native::bindings<uint32_t, haiku_font *> font_bindings;
    extern native::bindings<uint32_t, haiku_menu *> menu_bindings;
    extern native::bindings<
        native::app_wnd *,
        haiku_menu *> owner_menu_bindings;
    extern native::bindings<
        native::button *,
        haiku_button *> button_bindings;
}
