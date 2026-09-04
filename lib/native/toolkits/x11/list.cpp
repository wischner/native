//
// Implements the native Athena list control.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <stdexcept>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/List.h>
#include <native.h>
#include <native/list.h>
#include "globals.h"

namespace
{
    Widget list_parent(native::list *control) {
        auto *parent = control->get_parent();
        if (!parent || !parent->get_created())
            throw std::runtime_error(
                "X11/Athena: list requires a created parent.");
        Widget widget =
            linux::x11::wnd_bindings.handle_from_object(parent);
        if (!widget)
            throw std::runtime_error(
                "X11/Athena: list parent has no widget.");
        return widget;
    }
    void list_changed(Widget, XtPointer data, XtPointer call_data) {
        auto *result = static_cast<XawListReturnStruct *>(call_data);
        if (auto *owner = static_cast<native::list *>(data);
            owner && result && result->list_index >= 0)
            owner->on_native_selection(result->list_index);
    }
    void refresh_list(native::list *owner) {
        auto *binding =
            linux::x11::list_bindings.object_from_handle(owner);
        if (!binding || !binding->widget)
            throw std::runtime_error(
                "X11/Athena: Missing list widget.");
        binding->labels = owner->get_items();
        binding->pointers.clear();
        for (std::string &label : binding->labels)
            binding->pointers.push_back(label.data());
        XawListChange(binding->widget,
                      binding->pointers.empty()
                          ? nullptr
                          : binding->pointers.data(),
                      static_cast<int>(binding->pointers.size()),
                      0,
                      False);
    }
} // namespace

namespace native
{
    void list::apply_items() {
        refresh_list(this);
    }
    void list::apply_selected_index() {
        auto *binding =
            linux::x11::list_bindings.object_from_handle(this);
        if (!binding || !binding->widget)
            throw std::runtime_error(
                "X11/Athena: Missing list widget.");
        if (_selected_index < 0)
            XawListUnhighlight(binding->widget);
        else
            XawListHighlight(binding->widget, _selected_index);
    }
    void list::create_native() {
        Widget widget =
            XtVaCreateWidget("list",
                             listWidgetClass,
                             list_parent(this),
                             XtNhorizDistance,
                             _bounds.p.x,
                             XtNvertDistance,
                             _bounds.p.y,
                             XtNwidth,
                             linux::x11::widget_dimension(_bounds.d.w),
                             XtNheight,
                             linux::x11::widget_dimension(_bounds.d.h),
                             XtNdefaultColumns,
                             1,
                             XtNforceColumns,
                             True,
                             XtNverticalList,
                             True,
                             XtNleft,
                             XtChainLeft,
                             XtNright,
                             XtChainLeft,
                             XtNtop,
                             XtChainTop,
                             XtNbottom,
                             XtChainTop,
                             XtNresizable,
                             True,
                             nullptr);
        if (!widget)
            throw std::runtime_error(
                "X11/Athena: Failed to create list.");
        auto *self = this;
        auto *binding = new linux::x11::xaw_list();
        binding->widget = widget;
        linux::x11::wnd_bindings.register_pair(widget, self);
        linux::x11::list_bindings.register_pair(self, binding);
        XtAddCallback(widget, XtNcallback, list_changed, self);
        refresh_list(self);
        if (_selected_index >= 0)
            XawListHighlight(widget, _selected_index);
    }
    void list::show_native() {
        auto *binding = linux::x11::list_bindings.object_from_handle(
            this);
        if (!_created || !binding || !binding->widget)
            throw std::runtime_error(
                "X11/Athena: list is not created.");
        XtManageChild(binding->widget);
    }
    void list::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *binding =
            linux::x11::list_bindings.object_from_handle(self);
        if (binding) {
            if (binding->widget) {
                linux::x11::wnd_bindings.unregister_by_handle(
                    binding->widget);
                XtDestroyWidget(binding->widget);
            }
            linux::x11::list_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native
