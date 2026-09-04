//
// Implements the native Motif list control.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <stdexcept>
#include <X11/Intrinsic.h>
#include <Xm/List.h>
#include <Xm/ScrolledW.h>
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
        Widget w = linux::openmotif::parent_widget(c);
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

    Widget content_for(native::list *owner) {
        return linux::openmotif::list_content_bindings
            .object_from_handle(owner);
    }
} // namespace
namespace native
{
    void list::apply_items() {
        Widget w = content_for(this);
        if (!w)
            throw std::runtime_error("Motif: Missing list widget.");
        replace(w, _items);
    }
    void list::apply_selected_index() {
        Widget w = content_for(this);
        if (!w)
            throw std::runtime_error("Motif: Missing list widget.");
        XmListDeselectAllItems(w);
        if (_selected_index >= 0)
            XmListSelectPos(w, _selected_index + 1, False);
    }
    void list::create_native() {
        auto *self = this;
        Arg arguments[8];
        Cardinal count = 0;
        XtSetArg(arguments[count], XmNselectionPolicy,
                 XmBROWSE_SELECT); ++count;
        XtSetArg(arguments[count], XmNlistSizePolicy,
                 XmCONSTANT); ++count;
        XtSetArg(arguments[count], XmNnavigationType,
                 XmTAB_GROUP); ++count;
        Widget w = XmCreateScrolledList(
            parent_of(self),
            const_cast<char *>("list"),
            arguments,
            count);
        Widget scroller = w ? XtParent(w) : nullptr;
        if (!scroller || !w)
            throw std::runtime_error("Motif: Failed to create list.");
        XtVaSetValues(scroller,
                      XmNx, _bounds.p.x,
                      XmNy, _bounds.p.y,
                      XmNwidth, _bounds.d.w,
                      XmNheight, _bounds.d.h,
                      nullptr);
        XtManageChild(w);
        linux::openmotif::wnd_bindings.register_pair(scroller, self);
        linux::openmotif::list_content_bindings.register_pair(self, w);
        XtAddCallback(w, XmNbrowseSelectionCallback, changed, self);
        replace(w, _items);
        if (_selected_index >= 0)
            XmListSelectPos(w, _selected_index + 1, False);
    }
    void list::show_native() {
        Widget w = linux::openmotif::wnd_bindings.handle_from_object(
            this);
        if (!_created || !w)
            throw std::runtime_error("Motif: list is not created.");
        XtManageChild(w);
    }
    void list::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        Widget w =
            linux::openmotif::wnd_bindings.handle_from_object(self);
        Widget content = content_for(self);
        if (content)
            linux::openmotif::list_content_bindings
                .unregister_by_handle(self);
        if (w) {
            linux::openmotif::wnd_bindings.unregister_by_handle(w);
            XtDestroyWidget(w);
        }
    }
} // namespace native
