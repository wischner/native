//
// Implements the GEMix shared backend-state backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>

#include "globals.h"

namespace linux::gemix
{
    runtime_state runtime;
    native::bindings<WORD, native::wnd *> wnd_bindings;
    std::vector<native::button *> buttons;
    std::vector<native::check *> checks;
    std::vector<native::radio *> radios;
    std::vector<native::list *> lists;
    std::vector<native::combo_box *> combo_boxes;
    std::vector<native::text_edit *> text_edits;
    std::vector<native::accordion *> accordions;
    std::vector<native::tab_view *> tab_views;
    std::vector<native::icon_view *> icon_views;
    std::vector<native::tree_view *> tree_views;
    std::vector<native::table_view *> table_views;
    std::vector<native::code_edit *> code_edits;
    native::bindings<native::text_edit *, gem_text_edit *>
        text_edit_bindings;
    native::bindings<native::combo_box *, gem_combo_box *>
        combo_box_bindings;
    std::vector<native::panel *> panels;
    std::vector<native::canvas *> canvases;
    std::vector<native::app_wnd *> windows;
    native::app_wnd *active_window = nullptr;
    std::unordered_map<native::app_wnd *, menu_state> menu_states;

    bool ensure_runtime() {
        if (runtime.initialized)
            return true;

        runtime.appl_id = appl_init();
        if (runtime.appl_id < 0)
            return false;

        runtime.vdi_handle = gem_compat_graf_handle4(&runtime.char_w,
                                                     &runtime.char_h,
                                                     &runtime.box_w,
                                                     &runtime.box_h);
        v_opnvwk(
            runtime.work_in, &runtime.vdi_handle, runtime.work_out);
        if (runtime.vdi_handle == 0) {
            appl_exit();
            runtime.appl_id = -1;
            return false;
        }

        WORD distances[5] = {};
        WORD maximum_width = 0;
        if (vqt_fontinfo(runtime.vdi_handle,
                         nullptr,
                         nullptr,
                         distances,
                         &maximum_width,
                         nullptr)) {
            if (maximum_width > 0)
                runtime.char_w = maximum_width;
            if (distances[3] > 0)
                runtime.char_h = distances[3];
        }

        runtime.initialized = true;
        runtime.shutdown_requested = false;
        return true;
    }

    void shutdown_runtime() {
        if (!runtime.initialized)
            return;

        if (runtime.vdi_handle != 0) {
            v_clsvwk(runtime.vdi_handle);
            runtime.vdi_handle = 0;
        }

        appl_exit();
        runtime.appl_id = -1;
        runtime.initialized = false;
        runtime.shutdown_requested = false;
        wnd_bindings.clear();
        buttons.clear();
        checks.clear();
        radios.clear();
        lists.clear();
        combo_boxes.clear();
        text_edits.clear();
        accordions.clear();
        tab_views.clear();
        icon_views.clear();
        tree_views.clear();
        table_views.clear();
        code_edits.clear();
        text_edit_bindings.clear();
        combo_box_bindings.clear();
        windows.clear();
        active_window = nullptr;
        menu_states.clear();
    }

    native::rect desktop_rect() {
        WORD x = 0;
        WORD y = 0;
        WORD w = 0;
        WORD h = 0;

        if (!ensure_runtime() ||
            !wind_get(0, WF_WORKXYWH, &x, &y, &w, &h))
            return {};

        if (w <= 0 || h <= 0)
            return {};

        return native::rect(x,
                            y,
                            static_cast<native::dim>(w),
                            static_cast<native::dim>(h));
    }

    native::rect work_rect(WORD handle) {
        WORD x = 0;
        WORD y = 0;
        WORD w = 0;
        WORD h = 0;

        if (handle <= 0 ||
            !wind_get(handle, WF_WORKXYWH, &x, &y, &w, &h))
            return {};

        return native::rect(
            x,
            y,
            static_cast<native::dim>(std::max<WORD>(0, w)),
            static_cast<native::dim>(std::max<WORD>(0, h)));
    }

    native::rect outer_rect(WORD handle) {
        WORD x = 0;
        WORD y = 0;
        WORD w = 0;
        WORD h = 0;

        if (handle <= 0 ||
            !wind_get(handle, WF_CURRXYWH, &x, &y, &w, &h))
            return {};

        return native::rect(
            x,
            y,
            static_cast<native::dim>(std::max<WORD>(0, w)),
            static_cast<native::dim>(std::max<WORD>(0, h)));
    }

    native::size outer_size_for(WORD handle,
                                const native::size &work) {
        WORD kind = 0;
        WORD ignored_y = 0;
        WORD ignored_w = 0;
        WORD ignored_h = 0;
        if (handle <= 0 || !wind_get(handle,
                                     WF_KIND,
                                     &kind,
                                     &ignored_y,
                                     &ignored_w,
                                     &ignored_h))
            return work;

        WORD x = 0;
        WORD y = 0;
        WORD w = 0;
        WORD h = 0;
        if (!wind_calc(WC_BORDER,
                       static_cast<UWORD>(kind),
                       0,
                       0,
                       static_cast<WORD>(work.w),
                       static_cast<WORD>(work.h),
                       &x,
                       &y,
                       &w,
                       &h))
            return work;

        return native::size(
            static_cast<native::dim>(std::max<WORD>(0, w)),
            static_cast<native::dim>(std::max<WORD>(0, h)));
    }

    native::rect screen_rect() {
        if (!ensure_runtime() || runtime.work_out[0] < 0 ||
            runtime.work_out[1] < 0)
            return {};

        return native::rect(
            0,
            0,
            static_cast<native::dim>(runtime.work_out[0] + 1),
            static_cast<native::dim>(runtime.work_out[1] + 1));
    }
} // namespace linux::gemix
