//
// Implements single-selection lists with the native WINGs list widget.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <WINGs/WINGs.h>

#include <native/list.h>

#include "globals.h"

namespace
{
    WMList *widget_for(native::list *owner) {
        return reinterpret_cast<WMList *>(
            linux::wmaker::wnd_bindings.handle_from_object(owner));
    }

    void changed(WMWidget *widget, void *client_data) {
        auto *owner = static_cast<native::list *>(client_data);
        if (owner && linux::wmaker::permit_input(owner)) {
            const int selection = WMGetListSelectedItemRow(
                reinterpret_cast<WMList *>(widget));
            linux::wmaker::defer([owner, selection]() {
                if (owner->get_created())
                    owner->on_native_selection(selection);
            });
        }
    }
} // namespace

namespace native
{
    void list::apply_items() {
        WMList *widget = widget_for(this);
        if (!widget) {
            throw std::runtime_error(
                "Window Maker/WINGs: missing list binding.");
        }
        WMClearList(widget);
        for (const std::string &item : _items)
            WMAddListItem(widget, item.c_str());
    }

    void list::apply_selected_index() {
        WMList *widget = widget_for(this);
        if (!widget) {
            throw std::runtime_error(
                "Window Maker/WINGs: missing list binding.");
        }
        WMUnselectAllListItems(widget);
        if (_selected_index >= 0)
            WMSelectListItem(widget, _selected_index);
    }

    void list::create() const {
        if (_created)
            return;
        auto *self = const_cast<list *>(this);
        WMList *widget =
            WMCreateList(linux::wmaker::parent_widget(self));
        if (!widget) {
            throw std::runtime_error(
                "Window Maker/WINGs: unable to create a list.");
        }
        const point position =
            linux::wmaker::control_position(self);
        WMMoveWidget(widget, position.x, position.y);
        WMResizeWidget(widget, _bounds.d.w, _bounds.d.h);
        WMSetListAllowMultipleSelection(widget, False);
        WMSetListAllowEmptySelection(widget, True);
        for (const std::string &item : _items)
            WMAddListItem(widget, item.c_str());
        if (_selected_index >= 0)
            WMSelectListItem(widget, _selected_index);
        WMSetListAction(widget, changed, self);
        linux::wmaker::wnd_bindings.register_pair(widget, self);
        _created = true;
        self->on_wnd_create.emit();
    }

    void list::show() const {
        if (!_created) {
            throw std::runtime_error(
                "Window Maker/WINGs: cannot show an uncreated list.");
        }
        WMMapWidget(widget_for(const_cast<list *>(this)));
    }

    void list::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<list *>(this);
        WMList *widget = widget_for(self);
        self->on_native_destroy();
        linux::wmaker::wnd_bindings.unregister_by_object(self);
        if (widget)
            WMDestroyWidget(widget);
    }
} // namespace native
