//
// Implements the native Motif radio control.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <stdexcept>
#include <X11/Intrinsic.h>
#include <Xm/ToggleB.h>
#include <native.h>
#include <native/radio.h>
#include "globals.h"
namespace
{
    Widget parent_of(native::radio *c) {
        auto *p = c->get_parent();
        if (!p || !p->get_created())
            throw std::runtime_error(
                "Motif: radio requires a created parent.");
        Widget w = linux::openmotif::parent_widget(c);
        if (!w)
            throw std::runtime_error(
                "Motif: radio parent has no widget.");
        return w;
    }
    void changed(Widget, XtPointer d, XtPointer) {
        if (auto *o = static_cast<native::radio *>(d))
            o->on_native_selected();
    }
} // namespace
namespace native
{
    void radio::apply_text() {
        Widget w =
            linux::openmotif::wnd_bindings.handle_from_object(this);
        if (!w)
            throw std::runtime_error("Motif: Missing radio widget.");
        XmString s =
            XmStringCreateLocalized(const_cast<char *>(_text.c_str()));
        XtVaSetValues(w, XmNlabelString, s, nullptr);
        XmStringFree(s);
    }
    void radio::apply_selected() {
        Widget w =
            linux::openmotif::wnd_bindings.handle_from_object(this);
        if (!w)
            throw std::runtime_error("Motif: Missing radio widget.");
        XmToggleButtonSetState(w, _selected ? True : False, False);
    }
    void radio::create_native() {
        XmString s =
            XmStringCreateLocalized(const_cast<char *>(_text.c_str()));
        Widget w =
            XtVaCreateWidget("radio",
                             xmToggleButtonWidgetClass,
                             parent_of(this),
                             XmNx,
                             _bounds.p.x,
                             XmNy,
                             _bounds.p.y,
                             XmNwidth,
                             _bounds.d.w,
                             XmNheight,
                             _bounds.d.h,
                             XmNlabelString,
                             s,
                             XmNindicatorType,
                             XmONE_OF_MANY,
                             XmNset,
                             _selected ? True : False,
                             nullptr);
        XmStringFree(s);
        if (!w)
            throw std::runtime_error("Motif: Failed to create radio.");
        auto *self = this;
        XtAddCallback(w, XmNvalueChangedCallback, changed, self);
        linux::openmotif::wnd_bindings.register_pair(w, self);
    }
    void radio::show_native() {
        Widget w = linux::openmotif::wnd_bindings.handle_from_object(
            this);
        if (!_created || !w)
            throw std::runtime_error("Motif: radio is not created.");
        XtManageChild(w);
    }
    void radio::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        Widget w =
            linux::openmotif::wnd_bindings.handle_from_object(self);
        if (w) {
            linux::openmotif::wnd_bindings.unregister_by_handle(w);
            XtDestroyWidget(w);
        }
    }
} // namespace native
