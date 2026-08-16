//
// Implements check controls with the native WINGs switch button.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <WINGs/WINGs.h>

#include <native/check.h>

#include "globals.h"

namespace
{
    WMButton *widget_for(native::check *owner) {
        return reinterpret_cast<WMButton *>(
            linux::wmaker::wnd_bindings.handle_from_object(owner));
    }

    void changed(WMWidget *widget, void *client_data) {
        auto *owner = static_cast<native::check *>(client_data);
        if (owner && linux::wmaker::permit_input(owner)) {
            const bool selected = WMGetButtonSelected(
                                      reinterpret_cast<WMButton *>(
                                          widget)) != 0;
            linux::wmaker::defer([owner, selected]() {
                if (owner->get_created())
                    owner->on_native_checked(selected);
            });
        }
    }
} // namespace

namespace native
{
    void check::apply_text() {
        WMButton *widget = widget_for(this);
        if (!widget) {
            throw std::runtime_error(
                "Window Maker/WINGs: missing check binding.");
        }
        WMSetButtonText(widget, _text.c_str());
    }

    void check::apply_checked() {
        WMButton *widget = widget_for(this);
        if (!widget) {
            throw std::runtime_error(
                "Window Maker/WINGs: missing check binding.");
        }
        WMSetButtonSelected(widget, _checked);
    }

    void check::create() const {
        if (_created)
            return;
        auto *self = const_cast<check *>(this);
        WMButton *widget =
            WMCreateSwitchButton(linux::wmaker::parent_widget(self));
        if (!widget) {
            throw std::runtime_error(
                "Window Maker/WINGs: unable to create a check.");
        }
        const point position =
            linux::wmaker::control_position(self);
        WMMoveWidget(widget, position.x, position.y);
        WMResizeWidget(widget, _bounds.d.w, _bounds.d.h);
        WMSetWidgetBackgroundColor(
            widget, WMWhiteColor(WMWidgetScreen(widget)));
        WMSetButtonText(widget, _text.c_str());
        WMSetButtonSelected(widget, _checked);
        WMSetButtonAction(widget, changed, self);
        linux::wmaker::wnd_bindings.register_pair(widget, self);
        _created = true;
        self->on_wnd_create.emit();
    }

    void check::show() const {
        if (!_created) {
            throw std::runtime_error(
                "Window Maker/WINGs: cannot show an uncreated check.");
        }
        WMMapWidget(widget_for(const_cast<check *>(this)));
    }

    void check::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<check *>(this);
        WMButton *widget = widget_for(self);
        self->on_native_destroy();
        linux::wmaker::wnd_bindings.unregister_by_object(self);
        if (widget)
            WMDestroyWidget(widget);
    }
} // namespace native
