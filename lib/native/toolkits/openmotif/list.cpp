//
// Implements the native Motif list control.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <stdexcept>
#include <X11/Intrinsic.h>
#include <Xm/List.h>
#include <native.h>
#include <native/list.h>
#include "globals.h"
namespace
{
    Widget parent_of(native::list *c) {
        auto *p = c->get_parent();
        if (!p || !p->get_created())
            throw std::runtime_error(
                "Motif: list requires a created parent.");
        Widget w = linux::openmotif::wnd_bindings.handle_from_object(p);
        if (!w)
            throw std::runtime_error(
                "Motif: list parent has no widget.");
        return w;
    }
    void changed(Widget, XtPointer d, XtPointer call) {
        auto *o = static_cast<native::list *>(d);
        auto *r = static_cast<XmListCallbackStruct *>(call);
        if (o && r && r->item_position > 0)
            o->on_native_selection(r->item_position - 1);
    }
    void replace(Widget w, const std::vector<std::string> &items) {
        XmListDeleteAllItems(w);
        std::vector<XmString> values;
        for (const auto &i : items)
            values.push_back(
                XmStringCreateLocalized(const_cast<char *>(i.c_str())));
        if (!values.empty())
            XmListAddItems(
                w, values.data(), static_cast<int>(values.size()), 1);
        for (auto v : values)
            XmStringFree(v);
    }
} // namespace
namespace native
{
    void list::apply_items() {
        Widget w =
            linux::openmotif::wnd_bindings.handle_from_object(this);
        if (!w)
            throw std::runtime_error("Motif: Missing list widget.");
        replace(w, _items);
    }
    void list::apply_selected_index() {
        Widget w =
            linux::openmotif::wnd_bindings.handle_from_object(this);
        if (!w)
            throw std::runtime_error("Motif: Missing list widget.");
        XmListDeselectAllItems(w);
        if (_selected_index >= 0)
            XmListSelectPos(w, _selected_index + 1, False);
    }
    void list::create() const {
        if (_created)
            return;
        Widget w = XtVaCreateWidget("list",
                                    xmListWidgetClass,
                                    parent_of(const_cast<list *>(this)),
                                    XmNx,
                                    _bounds.p.x,
                                    XmNy,
                                    _bounds.p.y,
                                    XmNwidth,
                                    _bounds.d.w,
                                    XmNheight,
                                    _bounds.d.h,
                                    XmNselectionPolicy,
                                    XmBROWSE_SELECT,
                                    XmNlistSizePolicy,
                                    XmCONSTANT,
                                    nullptr);
        if (!w)
            throw std::runtime_error("Motif: Failed to create list.");
        auto *self = const_cast<list *>(this);
        linux::openmotif::wnd_bindings.register_pair(w, self);
        XtAddCallback(w, XmNbrowseSelectionCallback, changed, self);
        replace(w, _items);
        XtVaSetValues(w,
                      XmNwidth,
                      _bounds.d.w,
                      XmNheight,
                      _bounds.d.h,
                      nullptr);
        if (_selected_index >= 0)
            XmListSelectPos(w, _selected_index + 1, False);
        _created = true;
        self->on_wnd_create.emit();
    }
    void list::show() const {
        Widget w = linux::openmotif::wnd_bindings.handle_from_object(
            const_cast<list *>(this));
        if (!_created || !w)
            throw std::runtime_error("Motif: list is not created.");
        XtManageChild(w);
    }
    void list::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<list *>(this);
        Widget w =
            linux::openmotif::wnd_bindings.handle_from_object(self);
        self->on_native_destroy();
        if (w) {
            linux::openmotif::wnd_bindings.unregister_by_handle(w);
            XtDestroyWidget(w);
        }
    }
} // namespace native
