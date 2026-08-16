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
    extern std::vector<native::text_edit *> text_edits;
    extern std::vector<native::app_wnd *> windows;
    extern native::app_wnd *active_window;
    extern std::unordered_map<native::app_wnd *, menu_state>
        menu_states;

    struct gem_text_edit
    {
        std::size_t cursor = 0;
        std::size_t anchor = 0;
        bool visible = false;
        bool focused = false;
    };

    extern native::bindings<native::text_edit *, gem_text_edit *>
        text_edit_bindings;

    // Focus and position the editor under a local window point.
    bool focus_text_edit(native::app_wnd *parent, native::point point);

    // Route one AES keyboard packet to the focused editor.
    bool handle_text_edit_key(native::app_wnd *parent,
                              WORD modifiers,
                              WORD key);

    // Render every visible text editor belonging to a window.
    void render_text_edits(native::app_wnd *parent, native::gpx &g);

    // Select the AES text cursor while the pointer is over an editor.
    void update_text_edit_cursor(native::app_wnd *parent,
                                 native::point point);

    // Initialize AES and VDI once; return whether both are available.
    bool ensure_runtime();

    // Release process-wide AES and VDI state.
    void shutdown_runtime();

    // Return the GEM desktop work area.
    native::rect desktop_rect();

    // Return the full VDI screen bounds.
    native::rect screen_rect();

    // Request repainting for a native window.
    void request_repaint(native::wnd *target,
                         const native::rect *area = nullptr);

    // Return the object tree attached to an application window.
    OBJECT *menu_tree_for(native::app_wnd *owner);

    // Map a GEM menu object index to a public command ID.
    int menu_item_id_for(native::app_wnd *owner, WORD object_index);

    // Release the menu tree owned by an application window.
    void destroy_menu(native::app_wnd *owner);
} // namespace linux::gemix
