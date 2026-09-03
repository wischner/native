// Implements portable tabs in an OPEN LOOK collection panel.

#include <stdexcept>
#include <native.h>
#include <X11/Xlib.h>
#include <xview/xview.h>
#include "collection_host.h"
#include "globals.h"

namespace native
{
    void tab_view::apply_items() {
        auto *state = linux::openlook::tab_view_bindings
            .object_from_handle(this);
        if (state && state->content_panel) {
            const rect content = get_content_bounds();
            const int panel_x = static_cast<int>(
                xv_get(state->panel, XV_X));
            const int panel_y = static_cast<int>(
                xv_get(state->panel, XV_Y));
            xv_set(state->content_panel,
                   XV_X, panel_x + content.p.x,
                   XV_Y, panel_y + content.p.y,
                   XV_WIDTH, content.d.w,
                   XV_HEIGHT, content.d.h,
                   nullptr);
        }
        invalidate();
    }
    void tab_view::apply_selected_index() { invalidate(); }

    void tab_view::create() const {
        if (_created) return;
        auto *self = const_cast<tab_view *>(this);
        auto *state = linux::openlook::create_collection_panel(*self);
        linux::openlook::tab_view_bindings.register_pair(self, state);
        _created = true;
        self->synchronize_theme_metrics();
        const rect content = get_content_bounds();
        const int panel_x = static_cast<int>(xv_get(state->panel, XV_X));
        const int panel_y = static_cast<int>(xv_get(state->panel, XV_Y));
        state->content_panel = static_cast<Panel>(xv_create(
            xv_get(state->panel, XV_OWNER),
            PANEL,
            PANEL_LAYOUT, PANEL_HORIZONTAL,
            PANEL_BORDER, FALSE,
            XV_X, panel_x + content.p.x,
            XV_Y, panel_y + content.p.y,
            XV_WIDTH, content.d.w,
            XV_HEIGHT, content.d.h,
            XV_SHOW, FALSE,
            nullptr));
        if (!state->content_panel) {
            linux::openlook::destroy_collection_panel(*self, state);
            linux::openlook::tab_view_bindings.unregister_by_handle(self);
            throw std::runtime_error(
                "OpenLook/XView: failed to create tab content panel.");
        }
        self->configure_page_host(true, false);
        self->refresh();
        self->on_native_create();
    }

    void tab_view::show() const {
        auto *state = linux::openlook::tab_view_bindings.object_from_handle(
            const_cast<tab_view *>(this));
        if (!_created || !state || !state->panel)
            throw std::runtime_error("OpenLook/XView: tab_view is not created.");
        xv_set(state->panel, XV_SHOW, TRUE, nullptr);
        const Window window = static_cast<Window>(xv_get(state->panel, XV_XID));
        if (window != None && linux::openlook::cached_display) {
            XRaiseWindow(linux::openlook::cached_display, window);
        }
        xv_set(state->content_panel, XV_SHOW, TRUE, nullptr);
        const Window content_window = static_cast<Window>(
            xv_get(state->content_panel, XV_XID));
        if (content_window != None && linux::openlook::cached_display)
            XRaiseWindow(linux::openlook::cached_display, content_window);
        if (linux::openlook::cached_display)
            XFlush(linux::openlook::cached_display);
        const int selected = get_selected_index();
        if (selected >= 0) {
            wnd &content = get_item(
                static_cast<std::size_t>(selected)).get_content();
            if (content.get_created()) content.show();
        }
    }

    void tab_view::destroy() const {
        if (!_created) return;
        auto *self = const_cast<tab_view *>(this);
        auto *state = linux::openlook::tab_view_bindings
            .object_from_handle(self);
        linux::openlook::destroy_collection_panel(*self, state);
        linux::openlook::tab_view_bindings.unregister_by_handle(self);
    }
} // namespace native
