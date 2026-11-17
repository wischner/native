//
// Implements split_view with Motif's native XmPanedWindow and sash.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>

#include <X11/Intrinsic.h>
#include <Xm/PanedW.h>
#include <Xm/SashP.h>

#include <native.h>

#include "globals.h"
#include <X11/cursorfont.h>

namespace
{
    linux::openmotif::motif_split_view *binding(
        native::split_view &owner) {
        return linux::openmotif::split_view_bindings
            .object_from_handle(&owner);
    }

    void splitter_event(Widget, XtPointer, XEvent *, Boolean *);

    void show_sashes(Widget paned, native::split_view *owner) {
        WidgetList children = nullptr;
        Cardinal count = 0;
        XtVaGetValues(paned,
                      XmNchildren, &children,
                      XmNnumChildren, &count,
                      nullptr);
        for (Cardinal index = 0; index < count; ++index) {
            if (!XmIsSash(children[index]))
                continue;
            const auto bar = owner->get_splitter_bounds();
            const bool horizontal = owner->get_orientation() ==
                native::split_orientation::horizontal;
            XtConfigureWidget(children[index],
                bar.p.x + (horizontal ? 0 : std::max<int>(0, bar.d.w / 2 - 4)),
                bar.p.y + (horizontal ? std::max<int>(0, bar.d.h / 2 - 4) : 0),
                horizontal ? std::max<int>(1, bar.d.w) : 8,
                horizontal ? 8 : std::max<int>(1, bar.d.h), 0);
            XtVaSetValues(children[index], XmNtraversalOn, True, nullptr);
            XtSetMappedWhenManaged(children[index], True);
            if (!XtIsManaged(children[index]))
                XtManageChild(children[index]);
            XtAddEventHandler(children[index],
                              ButtonPressMask | ButtonReleaseMask |
                                  PointerMotionMask | EnterWindowMask,
                              False,
                              splitter_event,
                              owner);
        }
    }

    // Both the native sash and the full divider use one portable drag
    // transaction, keeping native pane geometry and saved ratio in sync.
    void splitter_event(Widget source,
                        XtPointer data,
                        XEvent *event,
                        Boolean *dispatch) {
        auto *owner = static_cast<native::split_view *>(data);
        auto *state = owner ? binding(*owner) : nullptr;
        if (!owner || !state || !event)
            return;
        Widget paned = state->paned;
        if (event->type == EnterNotify) {
            const auto shape =
                owner->get_orientation() ==
                        native::split_orientation::horizontal
                    ? XC_sb_h_double_arrow
                    : XC_sb_v_double_arrow;
            const auto cursor =
                XCreateFontCursor(XtDisplay(source), shape);
            XDefineCursor(XtDisplay(source), XtWindow(source), cursor);
            XFreeCursor(XtDisplay(source), cursor);
            return;
        }
        native::point position(
            event->type == MotionNotify ? event->xmotion.x
                                        : event->xbutton.x,
            event->type == MotionNotify ? event->xmotion.y
                                        : event->xbutton.y);
        if (source != paned && XtIsRealized(source)) {
            Window child = None;
            int x = 0, y = 0;
            XTranslateCoordinates(XtDisplay(paned), XtWindow(source),
                XtWindow(paned), position.x, position.y, &x, &y, &child);
            position = native::point(x, y);
        }
        if (event->type == ButtonPress &&
            event->xbutton.button == Button1 &&
            owner->get_splitter_bounds().contains(position)) {
            state->dragging = true;
            if (dispatch)
                *dispatch = False;
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
            if (dispatch)
                *dispatch = False;
            owner->on_native_mouse_move(position);
        } else if (event->type == ButtonRelease &&
                   event->xbutton.button == Button1 &&
                   state->dragging) {
            if (dispatch)
                *dispatch = False;
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
        apply_ratio();
    }

    void split_view::apply_ratio() {
        auto *state = binding(*this);
        if (!state || !state->paned)
            return;
        state->suppress = true;
        // Motif can rearrange both children when the host changes. Commit
        // both resolved rectangles even if their portable caches agree.
        const auto configure = [](wnd &pane, const rect &bounds) {
            Widget widget = linux::openmotif::wnd_bindings
                .handle_from_object(&pane);
            if (widget)
                XtConfigureWidget(widget, bounds.p.x, bounds.p.y,
                    std::max<int>(1, bounds.d.w),
                    std::max<int>(1, bounds.d.h), 0);
        };
        configure(get_first(), get_first_bounds());
        configure(get_second(), get_second_bounds());
        state->suppress = false;
        show_sashes(state->paned, this);
    }

    void split_view::apply_minimums() {
        // The portable resolver clamps minimums to the available space.
        // Native constraints must permit those smaller allocations too.
        Widget first = linux::openmotif::wnd_bindings
            .handle_from_object(&get_first());
        Widget second = linux::openmotif::wnd_bindings
            .handle_from_object(&get_second());
        if (first)
            XtVaSetValues(first,
                          XmNpaneMinimum, 1,
                          XmNallowResize, False,
                          XmNskipAdjust, True,
                          XmNshowSash, True,
                          nullptr);
        if (second)
            XtVaSetValues(second,
                          XmNpaneMinimum, 1,
                          XmNallowResize, False,
                          XmNskipAdjust, True,
                          XmNshowSash, True,
                          nullptr);
        apply_ratio();
    }

    void split_view::apply_splitter_size() {
        auto *state = binding(*this);
        if (state && state->paned)
            XtVaSetValues(state->paned,
                          XmNspacing, get_splitter_size(),
                          XmNsashWidth, get_splitter_size(),
                          XmNsashHeight, get_splitter_size(),
                          nullptr);
        apply_ratio();
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
            // Native owns both pane rectangles; Motif must not perform
            // a second layout when a nested pane or sash changes.
            XmNrefigureMode, False,
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
        self->_content_hosts_are_panes = false;
        self->refresh_contents();
        self->apply_minimums();
        self->apply_ratio();
        show_sashes(paned, self);
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
        show_sashes(state->paned, this);
    }

    void split_view::destroy_native() {
        if (!_created)
            return;
        destroy_children();
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
