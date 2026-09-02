//
// Implements push buttons with the native WINGs command button.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <WINGs/WINGs.h>

#include <native/button.h>

#include "globals.h"

namespace
{
    void activate(WMWidget *, void *client_data) {
        auto *owner = static_cast<native::button *>(client_data);
        if (owner && linux::wmaker::permit_input(owner)) {
            linux::wmaker::defer([owner]() {
                if (owner->get_created())
                    owner->on_native_click();
            });
        }
    }
} // namespace

namespace native
{
    void button::apply_text() {
        auto *widget = reinterpret_cast<WMButton *>(
            linux::wmaker::wnd_bindings.handle_from_object(this));
        if (!widget) {
            throw std::runtime_error(
                "Window Maker/WINGs: missing button binding.");
        }
        WMSetButtonText(widget, _text.c_str());
    }

    void button::create() const {
        if (_created)
            return;
        auto *self = const_cast<button *>(this);
        WMButton *widget =
            WMCreateCommandButton(linux::wmaker::parent_widget(self));
        if (!widget) {
            throw std::runtime_error(
                "Window Maker/WINGs: unable to create a button.");
        }
        const point position =
            linux::wmaker::control_position(self);
        WMMoveWidget(widget, position.x, position.y);
        WMResizeWidget(widget, _bounds.d.w, _bounds.d.h);
        WMSetButtonText(widget, _text.c_str());
        WMSetButtonAction(widget, activate, self);
        linux::wmaker::wnd_bindings.register_pair(widget, self);
        _created = true;
        self->on_native_create();
    }

    void button::show() const {
        if (!_created) {
            throw std::runtime_error(
                "Window Maker/WINGs: cannot show an uncreated button.");
        }
        WMWidget *widget =
            linux::wmaker::wnd_bindings.handle_from_object(
                const_cast<button *>(this));
        WMRealizeWidget(widget);
        WMMapWidget(widget);
    }

    void button::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<button *>(this);
        WMWidget *widget =
            linux::wmaker::wnd_bindings.handle_from_object(self);
        self->on_native_destroy();
        linux::wmaker::wnd_bindings.unregister_by_object(self);
        if (widget)
            WMDestroyWidget(widget);
    }
} // namespace native
