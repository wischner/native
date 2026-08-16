//
// Declares internal GEMix shared backend-state types and state.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

#include <gem.h>

#include <native.h>
#include <bindings.h>

namespace linux::gemix
{
    // GEM callbacks expose numeric handles, so shared runtime state and
    // registries provide the object context needed for dispatch.
    struct runtime_state
    {
        bool initialized = false;
        bool shutdown_requested = false;
        WORD appl_id = -1;
        VDI_HANDLE vdi_handle = 0;
        WORD work_in[11] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2};
        WORD work_out[57] = {};
        WORD char_w = 8;
        WORD char_h = 16;
        WORD box_w = 8;
        WORD box_h = 16;
    };

    // Stores one generated AES menu tree and its stable string storage.
    struct menu_state
    {
        std::vector<OBJECT> tree;
        std::deque<std::string> strings;
        std::unordered_map<int, int> object_to_item_id;
        bool installed = false;
    };

    extern runtime_state runtime;
    extern native::bindings<WORD, native::wnd *> wnd_bindings;
    extern std::vector<native::button *> buttons;
    extern std::vector<native::check *> checks;
    extern std::vector<native::radio *> radios;
    extern std::vector<native::list *> lists;
    extern std::vector<native::app_wnd *> windows;
    extern std::unordered_map<native::app_wnd *, menu_state>
        menu_states;

    // Initialize AES and VDI once; return whether both are available.
    bool ensure_runtime();

    // Release process-wide AES and VDI state.
    void shutdown_runtime();

    // Return the GEM desktop work area.
    native::rect desktop_rect();

    // Return the full VDI screen bounds.
    native::rect screen_rect();

    // Request repainting for a native window.
    void request_repaint(native::wnd *target);

    // Return the object tree attached to an application window.
    OBJECT *menu_tree_for(native::app_wnd *owner);

    // Map a GEM menu object index to a public command ID.
    int menu_item_id_for(native::app_wnd *owner, WORD object_index);

    // Release the menu tree owned by an application window.
    void destroy_menu(native::app_wnd *owner);
} // namespace linux::gemix
