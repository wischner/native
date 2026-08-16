//
// Implements the native Motif check control.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <stdexcept>
#include <X11/Intrinsic.h>
#include <Xm/ToggleB.h>
#include <native.h>
#include <native/check.h>
#include "globals.h"
namespace
{
    Widget parent_of(native::check *c) {
        auto *p = c->get_parent();
        if (!p || !p->get_created())
            throw std::runtime_error(
                "Motif: check requires a created parent.");
        Widget w = linux::openmotif::wnd_bindings.handle_from_object(p);
        if (!w)
            throw std::runtime_error(
                "Motif: check parent has no widget.");
        return w;
    }
    void changed(Widget, XtPointer d, XtPointer call) {
        auto *o = static_cast<native::check *>(d);
        auto *r = static_cast<XmToggleButtonCallbackStruct *>(call);
        if (o && r)
            o->on_native_checked(r->set != 0);
    }
} // namespace
namespace native
{
    void check::apply_text() {
        Widget w =
            linux::openmotif::wnd_bindings.handle_from_object(this);
        if (!w)
            throw std::runtime_error("Motif: Missing check widget.");
        XmString s =
            XmStringCreateLocalized(const_cast<char *>(_text.c_str()));
        XtVaSetValues(w, XmNlabelString, s, nullptr);
        XmStringFree(s);
    }
    void check::apply_checked() {
        Widget w =
            linux::openmotif::wnd_bindings.handle_from_object(this);
        if (!w)
            throw std::runtime_error("Motif: Missing check widget.");
        XmToggleButtonSetState(w, _checked ? True : False, False);
    }
    void check::create() const {
        if (_created)
            return;
        XmString s =
            XmStringCreateLocalized(const_cast<char *>(_text.c_str()));
        Widget w =
            XtVaCreateWidget("check",
                             xmToggleButtonWidgetClass,
                             parent_of(const_cast<check *>(this)),
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
                             XmN_OF_MANY,
                             XmNset,
                             _checked ? True : False,
                             nullptr);
        XmStringFree(s);
        if (!w)
            throw std::runtime_error("Motif: Failed to create check.");
        auto *self = const_cast<check *>(this);
        XtAddCallback(w, XmNvalueChangedCallback, changed, self);
        linux::openmotif::wnd_bindings.register_pair(w, self);
        _created = true;
        self->on_wnd_create.emit();
    }
    void check::show() const {
        Widget w = linux::openmotif::wnd_bindings.handle_from_object(
            const_cast<check *>(this));
        if (!_created || !w)
            throw std::runtime_error("Motif: check is not created.");
        XtManageChild(w);
    }
    void check::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<check *>(this);
        Widget w =
            linux::openmotif::wnd_bindings.handle_from_object(self);
        self->on_native_destroy();
        if (w) {
            linux::openmotif::wnd_bindings.unregister_by_handle(w);
            XtDestroyWidget(w);
        }
    }
} // namespace native
