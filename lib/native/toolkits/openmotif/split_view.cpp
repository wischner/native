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

    void hide_sashes(Widget paned) {
        WidgetList children = nullptr;
        Cardinal count = 0;
        XtVaGetValues(paned,
                      XmNchildren, &children,
                      XmNnumChildren, &count,
                      nullptr);
        for (Cardinal index = 0; index < count; ++index) {
            if (!XmIsSash(children[index]))
                continue;
            XtVaSetValues(children[index],
                          XmNwidth, 1,
                          XmNheight, 1,
                          XmNborderWidth, 0,
                          XmNtraversalOn, False,
                          nullptr);
            XtSetMappedWhenManaged(children[index], False);
            if (XtIsManaged(children[index]))
                XtUnmanageChild(children[index]);
            if (XtIsRealized(children[index]))
                XtUnmapWidget(children[index]);
        }
    }

    // Let the complete quiet divider act as the drag target. Motif's
    // default sash is a small bordered square at one end of the divider,
    // which is easy to miss and visually separates the splitter from the
    // surrounding window.
    void splitter_event(Widget paned,
                        XtPointer data,
                        XEvent *event,
                        Boolean *) {
        auto *owner = static_cast<native::split_view *>(data);
        auto *state = owner ? binding(*owner) : nullptr;
        if (!owner || !state || !event)
            return;
        const native::point position(
            event->type == MotionNotify ? event->xmotion.x
                                        : event->xbutton.x,
            event->type == MotionNotify ? event->xmotion.y
                                        : event->xbutton.y);
        if (event->type == ButtonPress &&
            event->xbutton.button == Button1 &&
            owner->get_splitter_bounds().contains(position)) {
            state->dragging = true;
            owner->on_native_mouse_click(native::mouse_event(
                native::mouse_button::left,
                native::mouse_action::press,
                position));
            XGrabPointer(XtDisplay(paned),
                         XtWindow(paned),
                         False,
                         PointerMotionMask | ButtonReleaseMask,
                         GrabModeAsync,
                         GrabModeAsync,
                         None,
                         None,
                         event->xbutton.time);
        } else if (event->type == MotionNotify && state->dragging) {
            owner->on_native_mouse_move(position);
        } else if (event->type == ButtonRelease &&
                   event->xbutton.button == Button1 &&
                   state->dragging) {
            owner->on_native_mouse_click(native::mouse_event(
                native::mouse_button::left,
                native::mouse_action::release,
                position));
            state->dragging = false;
            XUngrabPointer(XtDisplay(paned), event->xbutton.time);
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
                          XmNshowSash, False,
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
                          XmNspacing, get_splitter_size(),
                          XmNsashWidth, get_splitter_size(),
                          XmNsashHeight, get_splitter_size(),
                          nullptr);
    }

    void split_view::create_native() {
        auto *self = this;
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
            XmNresizable, False,
            XmNmarginWidth, 0,
            XmNmarginHeight, 0,
            XmNorientation,
            get_orientation() == split_orientation::horizontal
                ? XmHORIZONTAL : XmVERTICAL,
            XmNseparatorOn, False,
            XmNspacing, get_splitter_size(),
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
        self->_content_hosts_are_panes = true;
        self->refresh_contents();
        self->apply_minimums();
        self->apply_ratio();
        hide_sashes(paned);
        XtAddEventHandler(paned,
                          ButtonPressMask | ButtonReleaseMask |
                              PointerMotionMask,
                          False,
                          splitter_event,
                          self);
    }

    void split_view::show_native() {
        auto *state = binding(*this);
        if (!_created || !state || !state->paned)
            throw std::runtime_error("Motif: split_view is not created.");
        XtManageChild(state->paned);
        get_first().show();
        get_second().show();
        XtVaSetValues(state->paned,
                      XmNwidth, get_dimensions().w,
                      XmNheight, get_dimensions().h,
                      nullptr);
        apply_ratio();
        hide_sashes(state->paned);
    }

    void split_view::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *state = binding(*self);
        if (state && state->paned) {
            linux::openmotif::wnd_bindings.unregister_by_handle(state->paned);
            XtDestroyWidget(state->paned);
        }
        linux::openmotif::split_view_bindings.unregister_by_handle(self);
        delete state;
    }
} // namespace native
