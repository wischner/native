//
// Implements single-selection lists with the native WINGs list widget.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <cstring>
#include <stdexcept>

#include <WINGs/WINGs.h>
#include <WINGs/WINGsP.h>

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

    // Preserve the real WMList control while replacing its historical white
    // selection fill with the same semantic selection used by Window Maker
    // collection and table controls.
    void draw_item(WMList *widget,
                   int,
                   Drawable target,
                   char *text,
                   int state,
                   WMRect *bounds) {
        if (!widget || !bounds)
            return;

        auto *screen = reinterpret_cast<W_Screen *>(
            WMWidgetScreen(widget));
        if (!screen)
            return;

        const bool selected = (state & WLDSSelected) != 0;
        const bool disabled = (state & WLDSDisabled) != 0;
        WMColor *background = selected
                                  ? linux::wmaker::
                                        list_selection_background
                                  : screen->gray;
        WMColor *foreground = selected
                                  ? linux::wmaker::list_selection_text
                                  : (disabled
                                         ? screen->darkGray
                                         : screen->black);
        if (!background)
            background = selected ? screen->darkGray : screen->gray;
        if (!foreground)
            foreground = selected ? screen->white : screen->black;

        XFillRectangle(screen->display,
                       target,
                       WMColorGC(background),
                       bounds->pos.x,
                       bounds->pos.y,
                       bounds->size.width,
                       bounds->size.height);
        const char *label = text ? text : "";
        W_PaintText(WMWidgetView(widget),
                    target,
                    screen->normalFont,
                    bounds->pos.x + 4,
                    bounds->pos.y,
                    bounds->size.width > 8
                        ? bounds->size.width - 8
                        : 0,
                    WALeft,
                    foreground,
                    False,
                    label,
                    static_cast<int>(std::strlen(label)));
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

    void list::create_native() {
        auto *self = this;
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
        WMSetWidgetBackgroundColor(
            widget,
            reinterpret_cast<W_Screen *>(
                linux::wmaker::screen)->gray);
        WMSetListUserDrawProc(widget, draw_item);
        WMSetListAllowMultipleSelection(widget, False);
        WMSetListAllowEmptySelection(widget, True);
        for (const std::string &item : _items)
            WMAddListItem(widget, item.c_str());
        if (_selected_index >= 0)
            WMSelectListItem(widget, _selected_index);
        WMSetListAction(widget, changed, self);
        linux::wmaker::wnd_bindings.register_pair(widget, self);
    }

    void list::show_native() {
        if (!_created) {
            throw std::runtime_error(
                "Window Maker/WINGs: cannot show an uncreated list.");
        }
        WMList *widget = widget_for(this);
        WMRealizeWidget(widget);
        WMMapWidget(widget);
    }

    void list::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        WMList *widget = widget_for(self);
        linux::wmaker::wnd_bindings.unregister_by_object(self);
        if (widget)
            WMDestroyWidget(widget);
    }
} // namespace native
