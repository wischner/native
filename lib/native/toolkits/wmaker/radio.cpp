//
// Implements mutually exclusive controls with native WINGs radio
// buttons.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <WINGs/WINGs.h>

#include <native/radio.h>

#include "globals.h"

namespace
{
    WMButton *widget_for(native::radio *owner) {
        return reinterpret_cast<WMButton *>(
            linux::wmaker::wnd_bindings.handle_from_object(owner));
    }

    void changed(WMWidget *, void *client_data) {
        auto *owner = static_cast<native::radio *>(client_data);
        if (owner && linux::wmaker::permit_input(owner)) {
            linux::wmaker::defer([owner]() {
                if (owner->get_created())
                    owner->on_native_selected();
            });
        }
    }
} // namespace

namespace native
{
    void radio::apply_text() {
        WMButton *widget = widget_for(this);
        if (!widget) {
            throw std::runtime_error(
                "Window Maker/WINGs: missing radio binding.");
        }
        WMSetButtonText(widget, _text.c_str());
    }

    void radio::apply_selected() {
        WMButton *widget = widget_for(this);
        if (!widget) {
            throw std::runtime_error(
                "Window Maker/WINGs: missing radio binding.");
        }
        WMSetButtonSelected(widget, _selected);
    }

    void radio::create() const {
        if (_created)
            return;
        auto *self = const_cast<radio *>(this);
        WMButton *widget =
            WMCreateRadioButton(linux::wmaker::parent_widget(self));
        if (!widget) {
            throw std::runtime_error(
                "Window Maker/WINGs: unable to create a radio.");
        }
        const point position =
            linux::wmaker::control_position(self);
        WMMoveWidget(widget, position.x, position.y);
        WMResizeWidget(widget, _bounds.d.w, _bounds.d.h);
        WMSetWidgetBackgroundColor(
            widget, WMWhiteColor(WMWidgetScreen(widget)));
        WMSetButtonText(widget, _text.c_str());
        WMSetButtonSelected(widget, _selected);
        WMSetButtonAction(widget, changed, self);
        linux::wmaker::wnd_bindings.register_pair(widget, self);
        _created = true;
        self->on_wnd_create.emit();
    }

    void radio::show() const {
        if (!_created) {
            throw std::runtime_error(
                "Window Maker/WINGs: cannot show an uncreated radio.");
        }
        WMMapWidget(widget_for(const_cast<radio *>(this)));
    }

    void radio::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<radio *>(this);
        WMButton *widget = widget_for(self);
        self->on_native_destroy();
        linux::wmaker::wnd_bindings.unregister_by_object(self);
        if (widget)
            WMDestroyWidget(widget);
    }
} // namespace native
