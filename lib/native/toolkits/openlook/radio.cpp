//
// Implements the portable radio control with an XView choice item.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <native.h>
#include <native/radio.h>

#include <xview/panel.h>
#include <xview/xview.h>

#include "globals.h"

namespace
{
    void changed(Panel_item item, int value, Event *) {
        auto *owner = reinterpret_cast<native::radio *>(
            xv_get(item, PANEL_CLIENT_DATA));
        if (!linux::openlook::permit_input(owner)) {
            if (owner) {
                xv_set(item,
                       PANEL_VALUE,
                       owner->get_selected() ? 0 : PANEL_NONE,
                       nullptr);
            }
        } else if (value == 0) {
            owner->on_native_selected();
        } else if (owner->get_selected()) {
            xv_set(item, PANEL_VALUE, 0, nullptr);
        }
    }

    Panel_item item_for(native::radio *owner) {
        return static_cast<Panel_item>(
            linux::openlook::wnd_bindings.handle_from_object(owner));
    }
} // namespace

namespace native
{
    void radio::apply_text() {
        Panel_item item = item_for(this);
        if (!item) {
            throw std::runtime_error(
                "OpenLook/XView: missing radio binding.");
        }
        xv_set(item,
               PANEL_CHOICE_STRING,
               0,
               _text.c_str(),
               nullptr);
    }

    void radio::apply_selected() {
        Panel_item item = item_for(this);
        if (!item) {
            throw std::runtime_error(
                "OpenLook/XView: missing radio binding.");
        }
        xv_set(item,
               PANEL_VALUE,
               _selected ? 0 : PANEL_NONE,
               nullptr);
    }

    void radio::create() const {
        if (_created)
            return;
        auto *self = const_cast<radio *>(this);
        Panel panel = linux::openlook::parent_panel(self);
        Panel_item item = static_cast<Panel_item>(xv_create(
            panel,
            PANEL_CHOICE,
            PANEL_CHOOSE_ONE,
            TRUE,
            PANEL_CHOOSE_NONE,
            TRUE,
            PANEL_CHOICE_STRINGS,
            _text.c_str(),
            nullptr,
            PANEL_VALUE,
            _selected ? 0 : PANEL_NONE,
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
                "OpenLook/XView: failed to create radio.");
        }
        linux::openlook::wnd_bindings.register_pair(item, self);
        _created = true;
        self->on_wnd_create.emit();
    }

    void radio::show() const {
        Panel_item item = item_for(const_cast<radio *>(this));
        if (!_created || !item) {
            throw std::runtime_error(
                "OpenLook/XView: radio is not created.");
        }
        xv_set(item, XV_SHOW, TRUE, nullptr);
    }

    void radio::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<radio *>(this);
        Panel_item item = item_for(self);
        self->on_native_destroy();
        if (item) {
            linux::openlook::wnd_bindings.unregister_by_handle(item);
            xv_destroy_safe(item);
        }
    }
} // namespace native
