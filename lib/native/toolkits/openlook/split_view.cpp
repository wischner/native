// Implements a two-pane XView split container.

#include <stdexcept>

#include <native.h>
#include <xview/panel.h>
#include <xview/xview.h>

#include "globals.h"

namespace
{
    linux::openlook::openlook_split_view *binding(
        native::split_view &owner) {
        return linux::openlook::split_view_bindings
            .object_from_handle(&owner);
    }

    void position(Panel panel, const native::rect &bounds) {
        if (!panel) return;
        xv_set(panel,
               XV_X, bounds.p.x,
               XV_Y, bounds.p.y,
               XV_WIDTH, bounds.d.w,
               XV_HEIGHT, bounds.d.h,
               nullptr);
    }
}

namespace native
{
    void split_view::apply_orientation() { apply_ratio(); }

    void split_view::apply_ratio() {
        auto *state = binding(*this);
        if (!state) return;
        position(state->first, get_first_bounds());
        position(state->second, get_second_bounds());
    }

    void split_view::apply_minimums() { apply_ratio(); }
    void split_view::apply_splitter_size() { apply_ratio(); }

    void split_view::create() const {
        if (_created) return;
        auto *self = const_cast<split_view *>(this);
        Panel parent = linux::openlook::parent_panel(self);
        auto *state = new linux::openlook::openlook_split_view();
        state->host = static_cast<Panel>(xv_create(
            parent, PANEL,
            PANEL_BORDER, FALSE,
            XV_X, _bounds.p.x,
            XV_Y, _bounds.p.y,
            XV_WIDTH, _bounds.d.w,
            XV_HEIGHT, _bounds.d.h,
            XV_SHOW, FALSE,
            nullptr));
        if (state->host) {
            state->first = static_cast<Panel>(xv_create(
                state->host, PANEL, PANEL_BORDER, FALSE,
                XV_SHOW, FALSE, nullptr));
            state->second = static_cast<Panel>(xv_create(
                state->host, PANEL, PANEL_BORDER, FALSE,
                XV_SHOW, FALSE, nullptr));
        }
        if (!state->host || !state->first || !state->second) {
            if (state->host) xv_destroy_safe(state->host);
            delete state;
            throw std::runtime_error(
                "OpenLook/XView: failed to create split view.");
        }
        linux::openlook::wnd_bindings.register_pair(state->host, self);
        linux::openlook::split_view_bindings.register_pair(self, state);
        _created = true;
        self->_content_hosts_are_panes = true;
        self->refresh_contents();
        self->apply_ratio();
        self->on_native_create();
    }

    void split_view::show() const {
        auto *state = binding(*const_cast<split_view *>(this));
        if (!_created || !state)
            throw std::runtime_error(
                "OpenLook/XView: split_view is not created.");
        xv_set(state->host, XV_SHOW, TRUE, nullptr);
        xv_set(state->first, XV_SHOW, TRUE, nullptr);
        xv_set(state->second, XV_SHOW, TRUE, nullptr);
        get_first().show();
        get_second().show();
    }

    void split_view::destroy() const {
        if (!_created) return;
        auto *self = const_cast<split_view *>(this);
        auto *state = binding(*self);
        self->on_native_destroy();
        if (state && state->host) {
            linux::openlook::wnd_bindings.unregister_by_handle(state->host);
            xv_destroy_safe(state->host);
        }
        linux::openlook::split_view_bindings.unregister_by_handle(self);
        delete state;
    }
} // namespace native
