#pragma once

#include <vector>

#include <gem.h>

#include <native.h>
#include <bindings.h>

namespace gemix
{
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

    extern runtime_state runtime;
    extern native::bindings<WORD, native::wnd *> wnd_bindings;
    extern std::vector<native::button *> buttons;

    bool ensure_runtime();
    void shutdown_runtime();
    native::rect desktop_rect();
    void request_repaint(native::wnd *target);
    OBJECT *menu_tree_for(native::app_wnd *owner);
    int menu_item_id_for(native::app_wnd *owner, WORD object_index);
    void destroy_menu(native::app_wnd *owner);
}
