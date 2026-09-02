//
// Implements X11 buttons with the Athena Command widget. Athena owns
// the button appearance, pointer state, keyboard translations, and
// activation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Form.h>

#include <native.h>
#include <native/button.h>

#include "globals.h"

namespace
{
    void button_activate(Widget, XtPointer client_data, XtPointer) {
        auto *owner = static_cast<native::button *>(client_data);
        if (owner)
            owner->on_native_click();
    }
} // namespace

namespace native
{
    void button::apply_text() {
        auto *binding =
            linux::x11::button_bindings.object_from_handle(this);
        if (!binding || !binding->widget)
            throw std::runtime_error(
                "X11/Athena: Missing button widget binding.");

        XtVaSetValues(
            binding->widget, XtNlabel, _text.c_str(), nullptr);
    }

    void button::create() const {
        if (_created)
            return;

        wnd *parent = get_parent();
        if (!parent)
            throw std::runtime_error(
                "X11/Athena: button requires a parent window.");
        if (!parent->get_created())
            throw std::runtime_error(
                "X11/Athena: button parent is not created.");

        Widget parent_widget =
            linux::x11::wnd_bindings.handle_from_object(parent);
        if (!parent_widget)
            throw std::runtime_error(
                "X11/Athena: button parent has no widget.");

        Widget widget = XtVaCreateWidget("button",
                                         commandWidgetClass,
                                         parent_widget,
                                         XtNhorizDistance,
                                         _bounds.p.x,
                                         XtNvertDistance,
                                         _bounds.p.y,
                                         XtNwidth,
                                         _bounds.d.w,
                                         XtNheight,
                                         _bounds.d.h,
                                         XtNlabel,
                                         _text.c_str(),
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
                "X11/Athena: Failed to create Command widget.");

        auto *self = const_cast<button *>(this);
        XtAddCallback(widget, XtNcallback, button_activate, self);

        linux::x11::wnd_bindings.register_pair(widget, self);

        auto *binding = new linux::x11::xaw_button();
        binding->widget = widget;
        binding->owner = self;
        linux::x11::button_bindings.register_pair(self, binding);

        _created = true;
        self->on_native_create();
    }

    void button::show() const {
        if (!_created)
            throw std::runtime_error(
                "X11/Athena: Cannot show button before creation.");

        auto *binding = linux::x11::button_bindings.object_from_handle(
            const_cast<button *>(this));
        if (!binding || !binding->widget)
            throw std::runtime_error(
                "X11/Athena: Missing button widget binding.");

        XtManageChild(binding->widget);
    }

    void button::destroy() const {
        if (!_created)
            return;

        auto *self = const_cast<button *>(this);
        auto *binding =
            linux::x11::button_bindings.object_from_handle(self);
        self->on_native_destroy();

        if (binding) {
            if (binding->widget) {
                linux::x11::wnd_bindings.unregister_by_handle(
                    binding->widget);
                XtDestroyWidget(binding->widget);
            }
            linux::x11::button_bindings.unregister_by_handle(self);
            delete binding;
        }
    }
} // namespace native
