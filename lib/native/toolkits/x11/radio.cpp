//
// Implements the native Athena radio control.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <stdexcept>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Toggle.h>
#include <native.h>
#include <native/radio.h>
#include "globals.h"

namespace
{
    Widget radio_parent(native::radio *control) {
        auto *parent = control->get_parent();
        if (!parent || !parent->get_created())
            throw std::runtime_error(
                "X11/Athena: radio requires a created parent.");
        Widget widget =
            linux::x11::wnd_bindings.handle_from_object(parent);
        if (!widget)
            throw std::runtime_error(
                "X11/Athena: radio parent has no widget.");
        return widget;
    }
    void radio_changed(Widget, XtPointer data, XtPointer) {
        if (auto *owner = static_cast<native::radio *>(data))
            owner->on_native_selected();
    }
} // namespace

namespace native
{
    void radio::apply_text() {
        Widget widget =
            linux::x11::wnd_bindings.handle_from_object(this);
        if (!widget)
            throw std::runtime_error(
                "X11/Athena: Missing radio widget.");
        XtVaSetValues(widget, XtNlabel, _text.c_str(), nullptr);
    }
    void radio::apply_selected() {
        Widget widget =
            linux::x11::wnd_bindings.handle_from_object(this);
        if (!widget)
            throw std::runtime_error(
                "X11/Athena: Missing radio widget.");
        XtVaSetValues(
            widget, XtNstate, _selected ? True : False, nullptr);
    }
    void radio::create() const {
        if (_created)
            return;
        auto *self = const_cast<radio *>(this);
        Widget group = nullptr;
        for (wnd *sibling : _parent->_children) {
            if (sibling == this || !dynamic_cast<radio *>(sibling))
                continue;
            group =
                linux::x11::wnd_bindings.handle_from_object(sibling);
            if (group)
                break;
        }
        Widget widget =
            XtVaCreateWidget("radio",
                             toggleWidgetClass,
                             radio_parent(const_cast<radio *>(this)),
                             XtNhorizDistance,
                             _bounds.p.x,
                             XtNvertDistance,
                             _bounds.p.y,
                             XtNwidth,
                             linux::x11::widget_dimension(_bounds.d.w),
                             XtNheight,
                             linux::x11::widget_dimension(_bounds.d.h),
                             XtNlabel,
                             _text.c_str(),
                             XtNstate,
                             _selected ? True : False,
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
                             XtNradioGroup,
                             group,
                             nullptr);
        if (!widget)
            throw std::runtime_error(
                "X11/Athena: Failed to create radio.");
        XtAddCallback(widget, XtNcallback, radio_changed, self);
        linux::x11::wnd_bindings.register_pair(widget, self);
        _created = true;
        self->on_native_create();
    }
    void radio::show() const {
        Widget widget = linux::x11::wnd_bindings.handle_from_object(
            const_cast<radio *>(this));
        if (!_created || !widget)
            throw std::runtime_error(
                "X11/Athena: radio is not created.");
        XtManageChild(widget);
    }
    void radio::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<radio *>(this);
        Widget widget =
            linux::x11::wnd_bindings.handle_from_object(self);
        self->on_native_destroy();
        if (widget) {
            linux::x11::wnd_bindings.unregister_by_handle(widget);
            XtDestroyWidget(widget);
        }
    }
} // namespace native
