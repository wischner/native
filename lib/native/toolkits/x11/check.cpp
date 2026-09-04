//
// Implements the native Athena check control.
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
#include <native/check.h>
#include "globals.h"

namespace
{
    Widget check_parent(native::check *control) {
        auto *parent = control->get_parent();
        if (!parent || !parent->get_created())
            throw std::runtime_error(
                "X11/Athena: check requires a created parent.");
        Widget widget =
            linux::x11::wnd_bindings.handle_from_object(parent);
        if (!widget)
            throw std::runtime_error(
                "X11/Athena: check parent has no widget.");
        return widget;
    }
    void check_changed(Widget widget, XtPointer data, XtPointer) {
        Boolean state = False;
        XtVaGetValues(widget, XtNstate, &state, nullptr);
        if (auto *owner = static_cast<native::check *>(data))
            owner->on_native_checked(state == True);
    }
} // namespace

namespace native
{
    void check::apply_text() {
        Widget widget =
            linux::x11::wnd_bindings.handle_from_object(this);
        if (!widget)
            throw std::runtime_error(
                "X11/Athena: Missing check widget.");
        XtVaSetValues(widget, XtNlabel, _text.c_str(), nullptr);
    }
    void check::apply_checked() {
        Widget widget =
            linux::x11::wnd_bindings.handle_from_object(this);
        if (!widget)
            throw std::runtime_error(
                "X11/Athena: Missing check widget.");
        XtVaSetValues(
            widget, XtNstate, _checked ? True : False, nullptr);
    }
    void check::create_native() {
        Widget widget =
            XtVaCreateWidget("check",
                             toggleWidgetClass,
                             check_parent(this),
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
                             _checked ? True : False,
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
                "X11/Athena: Failed to create check.");
        auto *self = this;
        XtAddCallback(widget, XtNcallback, check_changed, self);
        linux::x11::wnd_bindings.register_pair(widget, self);
    }
    void check::show_native() {
        Widget widget = linux::x11::wnd_bindings.handle_from_object(
            this);
        if (!_created || !widget)
            throw std::runtime_error(
                "X11/Athena: check is not created.");
        XtManageChild(widget);
    }
    void check::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        Widget widget =
            linux::x11::wnd_bindings.handle_from_object(self);
        if (widget) {
            linux::x11::wnd_bindings.unregister_by_handle(widget);
            XtDestroyWidget(widget);
        }
    }
} // namespace native
