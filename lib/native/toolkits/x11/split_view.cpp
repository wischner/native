//
// Implements split_view with Athena's native Paned widget.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Paned.h>

#include <native.h>

#include "globals.h"

namespace
{
    linux::x11::xaw_split_view *binding(native::split_view &owner) {
        return linux::x11::split_view_bindings
            .object_from_handle(&owner);
    }

    void cache_children(native::split_view &owner,
                        linux::x11::xaw_split_view &state) {
        state.first = linux::x11::wnd_bindings.handle_from_object(
            &owner.get_first());
        state.second = linux::x11::wnd_bindings.handle_from_object(
            &owner.get_second());
    }

    struct pane_constraints
    {
        int available;
        int first_minimum;
        int second_minimum;
        int first_maximum;
        int second_maximum;
    };

    pane_constraints constraints(const native::split_view &owner) {
        const int total = owner.get_orientation() ==
                                  native::split_orientation::horizontal
                              ? owner.get_dimensions().w
                              : owner.get_dimensions().h;
        const int available = std::max(
            1, total-static_cast<int>(owner.get_splitter_size()));
        const int first_minimum = std::min(
            available, static_cast<int>(owner.get_first_minimum()));
        const int second_minimum = std::min(
            available-first_minimum,
            static_cast<int>(owner.get_second_minimum()));
        return {available,
                first_minimum,
                second_minimum,
                std::max(first_minimum, available-second_minimum),
                std::max(second_minimum, available-first_minimum)};
    }

    void pane_resized(Widget widget,
                      XtPointer client_data,
                      XEvent *event,
                      Boolean *) {
        if (!event || event->type != ConfigureNotify)
            return;
        auto *owner = static_cast<native::split_view *>(client_data);
        auto *state = owner ? binding(*owner) : nullptr;
        if (!owner || !state || state->suppress ||
            widget != state->first || !state->paned) {
            return;
        }
        Dimension total_width = 0;
        Dimension total_height = 0;
        Dimension border = 0;
        XtVaGetValues(state->paned,
                      XtNwidth,
                      &total_width,
                      XtNheight,
                      &total_height,
                      XtNinternalBorderWidth,
                      &border,
                      nullptr);
        const int total = owner->get_orientation() ==
                                  native::split_orientation::horizontal
                              ? total_width
                              : total_height;
        const int available = std::max(1, total-static_cast<int>(border));
        const int first = owner->get_orientation() ==
                                  native::split_orientation::horizontal
                              ? event->xconfigure.width
                              : event->xconfigure.height;
        owner->on_native_ratio(std::clamp(
            static_cast<float>(first)/available, 0.0f, 1.0f));
    }
} // namespace

namespace native
{
    void split_view::apply_orientation() {
        auto *state = binding(*this);
        if (!state || !state->paned)
            return;
        XtVaSetValues(
            state->paned,
            XtNorientation,
            get_orientation() == split_orientation::horizontal
                ? XtorientHorizontal
                : XtorientVertical,
            nullptr);
        apply_minimums();
        apply_ratio();
    }

    void split_view::apply_ratio() {
        auto *state = binding(*this);
        if (!state || !state->paned)
            return;
        cache_children(*this, *state);
        if (!state->first || !state->second)
            return;
        const pane_constraints limits = constraints(*this);
        const int first = std::clamp(
            static_cast<int>(limits.available*get_ratio()),
            limits.first_minimum,
            limits.first_maximum);
        state->suppress = true;
        XtVaSetValues(state->first,
                      XtNpreferredPaneSize,
                      first,
                      XtNresizeToPreferred,
                      True,
                      nullptr);
        XtVaSetValues(state->second,
                      XtNpreferredPaneSize,
                      std::max(0, limits.available-first),
                      XtNresizeToPreferred,
                      True,
                      nullptr);
        state->suppress = false;
    }

    void split_view::apply_minimums() {
        auto *state = binding(*this);
        if (!state)
            return;
        cache_children(*this, *state);
        if (!state->first || !state->second)
            return;
        const pane_constraints limits = constraints(*this);
        XawPanedSetMinMax(
            state->first,
            limits.first_minimum,
            limits.first_maximum);
        XawPanedSetMinMax(
            state->second,
            limits.second_minimum,
            limits.second_maximum);
    }

    void split_view::apply_splitter_size() {
        auto *state = binding(*this);
        if (!state || !state->paned)
            return;
        XtVaSetValues(state->paned,
                      XtNinternalBorderWidth,
                      get_splitter_size(),
                      nullptr);
        apply_minimums();
        apply_ratio();
    }

    void split_view::create() const {
        if (_created)
            return;
        wnd *parent = get_parent();
        if (!parent || !parent->get_created())
            throw std::runtime_error(
                "X11/Athena: split_view requires a created parent.");
        Widget parent_widget =
            linux::x11::wnd_bindings.handle_from_object(parent);
        if (!parent_widget)
            throw std::runtime_error(
                "X11/Athena: split_view parent has no widget.");

        auto *self = const_cast<split_view *>(this);
        auto *state = new linux::x11::xaw_split_view();
        state->paned = XtVaCreateWidget(
            "splitView",
            panedWidgetClass,
            parent_widget,
            XtNhorizDistance,
            _bounds.p.x,
            XtNvertDistance,
            _bounds.p.y,
            XtNwidth,
            _bounds.d.w,
            XtNheight,
            _bounds.d.h,
            XtNorientation,
            get_orientation() == split_orientation::horizontal
                ? XtorientHorizontal
                : XtorientVertical,
            XtNinternalBorderWidth,
            get_splitter_size(),
            nullptr);
        if (!state->paned) {
            delete state;
            throw std::runtime_error(
                "X11/Athena: failed to create Paned split_view.");
        }
        linux::x11::wnd_bindings.register_pair(state->paned, self);
        linux::x11::split_view_bindings.register_pair(self, state);
        _created = true;
        self->refresh_contents();
        cache_children(*self, *state);
        if (!state->first || !state->second) {
            self->destroy();
            throw std::runtime_error(
                "X11/Athena: split_view panes have no native widgets.");
        }
        XtVaSetValues(state->first,
                      XtNallowResize,
                      True,
                      XtNshowGrip,
                      True,
                      nullptr);
        XtVaSetValues(state->second,
                      XtNallowResize,
                      True,
                      XtNshowGrip,
                      True,
                      nullptr);
        XtAddEventHandler(state->first,
                          StructureNotifyMask,
                          False,
                          pane_resized,
                          self);
        self->apply_minimums();
        self->apply_ratio();
        self->on_native_create();
    }

    void split_view::show() const {
        auto *state = binding(*const_cast<split_view *>(this));
        if (!_created || !state || !state->paned)
            throw std::runtime_error(
                "X11/Athena: split_view is not created.");
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
            linux::x11::wnd_bindings.unregister_by_handle(state->paned);
            XtDestroyWidget(state->paned);
        }
        linux::x11::split_view_bindings.unregister_by_handle(self);
        delete state;
    }
} // namespace native
