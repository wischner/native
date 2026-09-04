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

#include "../../wnd_peer.h"

#include "../../emulated_tree.h"

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
    extern std::vector<native::combo_box *> combo_boxes;
    extern std::vector<native::text_edit *> text_edits;
    extern std::vector<native::accordion *> accordions;
    extern std::vector<native::tab_view *> tab_views;
    extern std::vector<native::icon_view *> icon_views;
    extern std::vector<native::tree_view *> tree_views;
    extern std::vector<native::table_view *> table_views;
    extern std::vector<native::code_edit *> code_edits;
    extern std::vector<native::panel *> panels;
    extern std::vector<native::canvas *> canvases;
    extern std::vector<native::app_wnd *> windows;
    extern native::app_wnd *active_window;
    extern std::unordered_map<native::app_wnd *, menu_state>
        menu_states;

    // Activate a menu shortcut from one AES keyboard packet.
    bool handle_menu_key(native::app_wnd *owner,
                         WORD modifiers,
                         WORD key);

    struct gem_text_edit
    {
        std::size_t cursor = 0;
        std::size_t anchor = 0;
        bool visible = false;
        bool focused = false;
    };
    struct gem_combo_box
    {
        bool open = false;
        bool focused = false;
    };
    inline constexpr native::detail::peer_bindings<
        native::combo_box *, gem_combo_box *> combo_box_bindings;

    bool handle_combo_key(native::app_wnd *parent,
                          WORD modifiers,
                          WORD key);
    inline constexpr native::detail::peer_bindings<
        native::text_edit *, gem_text_edit *> text_edit_bindings;

    // Focus and position the editor under a local window point.
    bool focus_text_edit(native::app_wnd *parent, native::point point);

    // Route one AES keyboard packet to the focused editor.
    bool handle_text_edit_key(native::app_wnd *parent,
                              WORD modifiers,
                              WORD key);

    // Render every visible text editor belonging to a window.
    void render_text_edits(native::app_wnd *parent, native::gpx &g);

    //
    // Return the top-level window an emulated control belongs to.
    //
    // Notes:
    //      Emulated controls may sit any number of panels deep inside
    //      an AES window, so painting and dispatch match on the root
    //      rather than on the immediate parent.
    //
    using native::detail::origin_in_root;
    using native::detail::root_bounds;
    using native::detail::root_of;

    // Draw panel and canvas regions under every control they contain.
    void render_surfaces(native::app_wnd *parent, native::gpx &g);

    // Route a local pointer position to a canvas or panel region.
    bool dispatch_surface_click(native::app_wnd *parent,
                                native::point point,
                                bool pressed);
    bool dispatch_surface_move(native::app_wnd *parent,
                               native::point point);

    // Draw every created collection descendant of an AES window.
    void render_collections(native::app_wnd *parent, native::gpx &g);
    void render_tab_views(native::app_wnd *parent, native::gpx &g);

    // Route a local click release to a collection control.
    bool activate_collection(native::app_wnd *parent,
                             native::point point);

    // Route one AES key packet to the focused collection control.
    bool handle_collection_key(native::app_wnd *parent,
                               WORD modifiers,
                               WORD key);

    // Route pointer motion to a source editor under the pointer.
    bool update_collection_pointer(native::app_wnd *parent,
                                   native::point point);

    // Forget pending double-click state for a destroyed tree.
    void forget_tree_click(native::tree_view *control);

    // Select the cursor of the deepest visible window at a root point.
    void update_mouse_cursor(native::app_wnd *parent,
                             native::point point);

    // Initialize AES and VDI once; return whether both are available.
    bool ensure_runtime();

    // Release process-wide AES and VDI state.
    void shutdown_runtime();

    // Return the GEM desktop work area.
    native::rect desktop_rect();

    //
    // Return a window's work-area rectangle.
    //
    // Notes:
    //      AES subtracts every decoration the window carries: title,
    //      borders, info line, and the sliders and arrows when the
    //      window was created with them. This is the area the backend
    //      paints and hit-tests, so it is the size the portable layer
    //      is told about.
    //
    native::rect work_rect(WORD handle);

    // Return a window's outer rectangle, decorations included.
    native::rect outer_rect(WORD handle);

    //
    // Return the outer size whose work area is a requested size.
    //
    // Notes:
    //      The inverse of work_rect(), through wind_calc() and the
    //      window's own kind mask, so a window asked for a client of
    //      a given size is opened large enough to hold it whatever
    //      decorations it carries.
    //
    native::size outer_size_for(WORD handle, const native::size &work);

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
