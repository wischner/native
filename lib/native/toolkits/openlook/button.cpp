//
// Implements the portable button with the native XView Panel button.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <native.h>
#include <native/button.h>

#include <xview/panel.h>
#include <xview/xview.h>

#include "globals.h"

namespace
{
    void activate(Panel_item item, Event *) {
        auto *owner = reinterpret_cast<native::button *>(
            xv_get(item, PANEL_CLIENT_DATA));
        if (linux::openlook::permit_input(owner))
            owner->on_click.emit();
    }
} // namespace

namespace native
{
    void button::apply_text() {
        Panel_item item = static_cast<Panel_item>(
            linux::openlook::wnd_bindings.handle_from_object(this));
        if (!item) {
            throw std::runtime_error(
                "OpenLook/XView: missing button binding.");
        }
        xv_set(item, PANEL_LABEL_STRING, _text.c_str(), nullptr);
    }

    void button::create() const {
        if (_created)
            return;
        auto *self = const_cast<button *>(this);
        Panel panel = linux::openlook::parent_panel(self);
        Panel_item item = static_cast<Panel_item>(xv_create(
            panel,
            PANEL_BUTTON,
            PANEL_LABEL_STRING,
            _text.c_str(),
            PANEL_NOTIFY_PROC,
            activate,
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
                "OpenLook/XView: failed to create button.");
        }
        linux::openlook::wnd_bindings.register_pair(item, self);
        linux::openlook::fit_item_width(item, _bounds.d.w);
        _created = true;
        self->on_wnd_create.emit();
    }

    void button::show() const {
        Panel_item item = static_cast<Panel_item>(
            linux::openlook::wnd_bindings.handle_from_object(
                const_cast<button *>(this)));
        if (!_created || !item) {
            throw std::runtime_error(
                "OpenLook/XView: button is not created.");
        }
        xv_set(item, XV_SHOW, TRUE, nullptr);
    }

    void button::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<button *>(this);
        Panel_item item = static_cast<Panel_item>(
            linux::openlook::wnd_bindings.handle_from_object(self));
        self->on_native_destroy();
        if (item) {
            linux::openlook::wnd_bindings.unregister_by_handle(item);
            xv_destroy_safe(item);
        }
    }
} // namespace native
