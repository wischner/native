//
// Implements the portable check control with XView PANEL_CHECK_BOX.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <native.h>
#include <native/check.h>

#include <xview/panel.h>
#include <xview/xview.h>

#include "globals.h"

namespace
{
    void changed(Panel_item item, int value, Event *) {
        auto *owner = reinterpret_cast<native::check *>(
            xv_get(item, PANEL_CLIENT_DATA));
        if (linux::openlook::permit_input(owner)) {
            owner->on_native_checked((value & 1) != 0);
        } else if (owner) {
            xv_set(item,
                   PANEL_VALUE,
                   owner->get_checked() ? 1 : 0,
                   nullptr);
        }
    }

    Panel_item item_for(native::check *owner) {
        return static_cast<Panel_item>(
            linux::openlook::wnd_bindings.handle_from_object(owner));
    }
} // namespace

namespace native
{
    void check::apply_text() {
        Panel_item item = item_for(this);
        if (!item) {
            throw std::runtime_error(
                "OpenLook/XView: missing check binding.");
        }
        xv_set(item,
               PANEL_CHOICE_STRING,
               0,
               _text.c_str(),
               nullptr);
    }

    void check::apply_checked() {
        Panel_item item = item_for(this);
        if (!item) {
            throw std::runtime_error(
                "OpenLook/XView: missing check binding.");
        }
        xv_set(item, PANEL_VALUE, _checked ? 1 : 0, nullptr);
    }

    void check::create() const {
        if (_created)
            return;
        auto *self = const_cast<check *>(this);
        Panel panel = linux::openlook::parent_panel(self);
        Panel_item item = static_cast<Panel_item>(xv_create(
            panel,
            PANEL_CHECK_BOX,
            PANEL_CHOICE_STRINGS,
            _text.c_str(),
            nullptr,
            PANEL_VALUE,
            _checked ? 1 : 0,
            PANEL_NOTIFY_PROC,
            changed,
            PANEL_CLIENT_DATA,
            self,
            XV_X,
            _bounds.p.x,
            XV_Y,
            _bounds.p.y,
            XV_WIDTH,
            _bounds.d.w,
            XV_HEIGHT,
            _bounds.d.h,
            XV_SHOW,
            FALSE,
            nullptr));
        if (!item) {
            throw std::runtime_error(
                "OpenLook/XView: failed to create check.");
        }
        linux::openlook::wnd_bindings.register_pair(item, self);
        _created = true;
        self->on_native_create();
    }

    void check::show() const {
        Panel_item item = item_for(const_cast<check *>(this));
        if (!_created || !item) {
            throw std::runtime_error(
                "OpenLook/XView: check is not created.");
        }
        xv_set(item, XV_SHOW, TRUE, nullptr);
    }

    void check::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<check *>(this);
        Panel_item item = item_for(self);
        self->on_native_destroy();
        if (item) {
            linux::openlook::wnd_bindings.unregister_by_handle(item);
            xv_destroy_safe(item);
        }
    }
} // namespace native
