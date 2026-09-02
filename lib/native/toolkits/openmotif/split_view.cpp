// Implements split_view with Motif's native XmPanedWindow.

#include <algorithm>
#include <stdexcept>

#include <X11/Intrinsic.h>
#include <Xm/PanedW.h>
#include <Xm/SashP.h>

#include <native.h>

#include "globals.h"

namespace
{
    linux::openmotif::motif_split_view *binding(
        native::split_view &owner) {
        return linux::openmotif::split_view_bindings
            .object_from_handle(&owner);
    }

    void sash_changed(Widget, XtPointer data, XtPointer) {
        auto *owner = static_cast<native::split_view *>(data);
        auto *state = owner ? binding(*owner) : nullptr;
        if (!owner || !state || state->suppress)
            return;
        Widget first = linux::openmotif::wnd_bindings
            .handle_from_object(&owner->get_first());
        Widget second = linux::openmotif::wnd_bindings
            .handle_from_object(&owner->get_second());
        if (!first || !second)
            return;
        Dimension first_extent = 0;
        Dimension second_extent = 0;
        if (owner->get_orientation() ==
            native::split_orientation::horizontal) {
            XtVaGetValues(first, XmNwidth, &first_extent, nullptr);
            XtVaGetValues(second, XmNwidth, &second_extent, nullptr);
        } else {
            XtVaGetValues(first, XmNheight, &first_extent, nullptr);
            XtVaGetValues(second, XmNheight, &second_extent, nullptr);
        }
        const float total = first_extent + second_extent;
        if (total > 0)
            owner->on_native_ratio(first_extent/total);
    }

    void connect_sashes(native::split_view &owner, Widget paned) {
        WidgetList children = nullptr;
        Cardinal count = 0;
        XtVaGetValues(paned,
                      XmNchildren, &children,
                      XmNnumChildren, &count,
                      nullptr);
        for (Cardinal index = 0; index < count; ++index) {
            if (XmIsSash(children[index]))
                XtAddCallback(children[index], XmNvalueChangedCallback,
                              sash_changed, &owner);
        }
    }
} // namespace

namespace native
{
    void split_view::apply_orientation() {
        auto *state = binding(*this);
        if (state && state->paned)
            XtVaSetValues(state->paned, XmNorientation,
                get_orientation() == split_orientation::horizontal
                    ? XmHORIZONTAL : XmVERTICAL,
                nullptr);
    }

    void split_view::apply_ratio() {
        auto *state = binding(*this);
        if (!state || !state->paned)
            return;
        Widget first = linux::openmotif::wnd_bindings
            .handle_from_object(&get_first());
        if (!first)
            return;
        const int total = get_orientation() == split_orientation::horizontal
            ? static_cast<int>(get_dimensions().w)
            : static_cast<int>(get_dimensions().h);
        const int desired = std::max(1, static_cast<int>(
            (total-get_splitter_size())*get_ratio()));
        state->suppress = true;
        if (get_orientation() == split_orientation::horizontal)
            XtVaSetValues(first, XmNwidth, desired, nullptr);
        else
            XtVaSetValues(first, XmNheight, desired, nullptr);
        state->suppress = false;
    }

    void split_view::apply_minimums() {
        Widget first = linux::openmotif::wnd_bindings
            .handle_from_object(&get_first());
        Widget second = linux::openmotif::wnd_bindings
            .handle_from_object(&get_second());
        if (first)
            XtVaSetValues(first,
                          XmNpaneMinimum, get_first_minimum(),
                          XmNskipAdjust, False,
                          XmNshowSash, True,
                          nullptr);
        if (second)
            XtVaSetValues(second,
                          XmNpaneMinimum, get_second_minimum(),
                          XmNskipAdjust, False,
                          XmNshowSash, False,
                          nullptr);
    }

    void split_view::apply_splitter_size() {
        auto *state = binding(*this);
        if (state && state->paned)
            XtVaSetValues(state->paned,
                          XmNsashWidth, get_splitter_size(),
                          XmNsashHeight, get_splitter_size(),
                          nullptr);
    }

    void split_view::create() const {
        if (_created)
            return;
        auto *self = const_cast<split_view *>(this);
        Widget parent = linux::openmotif::parent_widget(self);
        if (!parent)
            throw std::runtime_error(
                "Motif: split_view requires a created parent.");
        Widget paned = XtVaCreateWidget(
            "splitView", xmPanedWindowWidgetClass, parent,
            XmNx, _bounds.p.x,
            XmNy, _bounds.p.y,
            XmNwidth, _bounds.d.w,
            XmNheight, _bounds.d.h,
            XmNorientation,
            get_orientation() == split_orientation::horizontal
                ? XmHORIZONTAL : XmVERTICAL,
            XmNseparatorOn, True,
            XmNsashWidth, get_splitter_size(),
            XmNsashHeight, get_splitter_size(),
            nullptr);
        if (!paned)
            throw std::runtime_error(
                "Motif: failed to create XmPanedWindow.");
        auto *state = new linux::openmotif::motif_split_view();
        state->paned = paned;
        linux::openmotif::wnd_bindings.register_pair(paned, self);
        linux::openmotif::split_view_bindings.register_pair(self, state);
        _created = true;
        self->_content_hosts_are_panes = true;
        self->refresh_contents();
        self->apply_minimums();
        self->apply_ratio();
        connect_sashes(*self, paned);
        self->on_native_create();
    }

    void split_view::show() const {
        auto *state = binding(*const_cast<split_view *>(this));
        if (!_created || !state || !state->paned)
            throw std::runtime_error("Motif: split_view is not created.");
        XtManageChild(state->paned);
        get_first().show();
        get_second().show();
    }

    void split_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<split_view *>(this);
        auto *state = binding(*self);
        self->on_native_destroy();
        if (state && state->paned) {
            linux::openmotif::wnd_bindings.unregister_by_handle(state->paned);
            XtDestroyWidget(state->paned);
        }
        linux::openmotif::split_view_bindings.unregister_by_handle(self);
        delete state;
    }
} // namespace native
