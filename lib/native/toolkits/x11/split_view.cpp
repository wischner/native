//
// Implements split_view with Athena's native Paned widget.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Paned.h>

#include <native.h>

#include "globals.h"

namespace
{
    void draw_grip(native::split_view &owner, Widget widget) {
        if (!XtIsRealized(widget)) return;
        const native::rect strip = owner.get_splitter_bounds();
        const bool horizontal = owner.get_orientation() == native::split_orientation::horizontal;
        if ((horizontal ? strip.d.w : strip.d.h) < 3 ||
            (horizontal ? strip.d.h : strip.d.w) < 15) return;
        XGCValues values{};
        XtVaGetValues(widget, XtNborderColor, &values.foreground, nullptr);
        GC gc = XtGetGC(widget, GCForeground, &values);
        const int x = strip.x1() + strip.d.w / 2;
        const int y = strip.y1() + strip.d.h / 2;
        for (int offset = -6; offset <= 6; offset += 4) {
            if (horizontal)
                XDrawLine(XtDisplay(widget), XtWindow(widget), gc,
                    x - 1, y + offset, x, y + offset);
            else
                XDrawLine(XtDisplay(widget), XtWindow(widget), gc,
                    x + offset, y - 1, x + offset, y);
        }
        XtReleaseGC(widget, gc);
    }

    linux::x11::xaw_split_view *binding(native::split_view &owner) {
        return linux::x11::split_view_bindings
            .object_from_handle(&owner);
    }

    void fit_child(native::wnd &child, Widget host) {
        if (!host || !child.get_created()) return;
        Dimension width = 1, height = 1;
        XtVaGetValues(host, XtNwidth, &width, XtNheight, &height, nullptr);
        child.set_bounds(native::rect(0, 0, width, height));
    }

    void pane_resized(Widget host, XtPointer data, XEvent *event, Boolean *) {
        if (event && event->type == ConfigureNotify)
            fit_child(*static_cast<native::wnd *>(data), host);
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
            2, total-static_cast<int>(owner.get_splitter_size()));
        const int first_minimum = std::min(
            available - 1, std::max(1, static_cast<int>(owner.get_first_minimum())));
        const int second_minimum = std::min(
            available-first_minimum,
            std::max(1, static_cast<int>(owner.get_second_minimum())));
        return {available,
                first_minimum,
                second_minimum,
                std::max(first_minimum, available-second_minimum),
                std::max(second_minimum, available-first_minimum)};
    }

    void separator_event(Widget widget,
                      XtPointer client_data,
                      XEvent *event,
                      Boolean *) {
        auto *owner = static_cast<native::split_view *>(client_data);
        auto *state = owner ? binding(*owner) : nullptr;
        if (!owner || !state || !event || state->suppress) return;
        if (event->type == Expose) {
            draw_grip(*owner, widget);
        } else if (event->type == ConfigureNotify) {
            Dimension width = 1, height = 1;
            XtVaGetValues(widget, XtNwidth, &width, XtNheight, &height, nullptr);
            owner->on_native_resize(native::size(width, height));
        } else if (event->type == MotionNotify) {
            owner->on_native_mouse_move(native::point(
                event->xmotion.x, event->xmotion.y));
        } else if ((event->type == ButtonPress || event->type == ButtonRelease) &&
                   event->xbutton.button == Button1) {
            if (event->type == ButtonPress) {
                // Use the complete exposed native separator, including its
                // leading pixel, then capture motion outside both panes.
                if (XtGrabPointer(widget, False,
                    ButtonReleaseMask | PointerMotionMask, GrabModeAsync,
                    GrabModeAsync, None, None, event->xbutton.time) != GrabSuccess)
                    return;
            }
            const auto divider = owner->get_splitter_bounds();
            const auto position = event->type == ButtonPress
                ? divider.p
                : native::point(event->xbutton.x, event->xbutton.y);
            if (event->type == ButtonRelease)
                XtUngrabPointer(widget, event->xbutton.time);
            owner->on_native_mouse_click(native::mouse_event(
                native::mouse_button::left,
                event->type == ButtonPress ? native::mouse_action::press
                                          : native::mouse_action::release,
                position));
        }
    }
} // namespace

namespace native
{
    void split_view::apply_orientation() {
        auto *state = binding(*this);
        if (!state || !state->paned)
            return;
        // Xaw resets pane sizes while changing axes. Refigure only after
        // constraints for the new axis have been installed together.
        XawPanedSetRefigureMode(state->paned, False);
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
        if (!state->first || !state->second)
            return;
        const pane_constraints limits = constraints(*this);
        const int first = std::clamp(
            static_cast<int>(std::lround(limits.available*get_ratio())),
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
        // Xaw does not refigure live panes when preferredPaneSize changes.
        // Commit both sizes together through its public constraint API,
        // then restore the permitted ranges without an intermediate frame.
        XawPanedSetRefigureMode(state->paned, False);
        XawPanedSetMinMax(state->first, first, first);
        const int second = std::max(0, limits.available - first);
        XawPanedSetMinMax(state->second, second, second);
        XawPanedSetRefigureMode(state->paned, True);
        XawPanedSetRefigureMode(state->paned, False);
        apply_minimums();
        XawPanedSetRefigureMode(state->paned, True);
        state->suppress = false;
        fit_child(get_first(), state->first);
        fit_child(get_second(), state->second);
    }

    void split_view::apply_minimums() {
        auto *state = binding(*this);
        if (!state)
            return;
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
        XawPanedSetRefigureMode(state->paned, False);
        XtVaSetValues(state->paned,
                      XtNinternalBorderWidth,
                      get_splitter_size(),
                      nullptr);
        apply_minimums();
        apply_ratio();
    }

    void split_view::create_native() {
        wnd *parent = get_parent();
        if (!parent || !parent->get_created())
            throw std::runtime_error(
                "X11/Athena: split_view requires a created parent.");
        Widget parent_widget =
            linux::x11::parent_widget(this);
        if (!parent_widget)
            throw std::runtime_error(
                "X11/Athena: split_view parent has no widget.");

        auto *self = this;
        auto *state = new linux::x11::xaw_split_view();
        Pixel background = 0;
        XtVaGetValues(parent_widget, XtNbackground, &background, nullptr);
        state->paned = XtVaCreateWidget(
            "splitView",
            panedWidgetClass,
            parent_widget,
            XtNhorizDistance,
            _bounds.p.x,
            XtNvertDistance,
            _bounds.p.y,
            XtNwidth,
            linux::x11::widget_dimension(_bounds.d.w),
            XtNheight,
            linux::x11::widget_dimension(_bounds.d.h),
            XtNorientation,
            get_orientation() == split_orientation::horizontal
                ? XtorientHorizontal
                : XtorientVertical,
            XtNinternalBorderWidth,
            get_splitter_size(),
            XtNinternalBorderColor, background,
            XtNbackground, background,
            XtNborderWidth, 0,
            nullptr);
        if (!state->paned) {
            delete state;
            throw std::runtime_error(
                "X11/Athena: failed to create Paned split_view.");
        }
        linux::x11::wnd_bindings.register_pair(state->paned, self);
        linux::x11::split_view_bindings.register_pair(self, state);
        state->first = XtVaCreateManagedWidget("firstPane",
            linux::x11::layout_host_class(), state->paned,
            XtNwidth, linux::x11::widget_dimension(get_first_bounds().d.w),
            XtNheight, linux::x11::widget_dimension(get_first_bounds().d.h),
            XtNpreferredPaneSize, std::max(1, resolved_first_extent()),
            XtNborderWidth, 0, XtNdefaultDistance, 0,
            XtNshowGrip, False, XtNallowResize, True, nullptr);
        state->second = XtVaCreateManagedWidget("secondPane",
            linux::x11::layout_host_class(), state->paned,
            XtNwidth, linux::x11::widget_dimension(get_second_bounds().d.w),
            XtNheight, linux::x11::widget_dimension(get_second_bounds().d.h),
            XtNpreferredPaneSize, std::max(1,
                constraints(*this).available - resolved_first_extent()),
            XtNborderWidth, 0, XtNdefaultDistance, 0,
            XtNshowGrip, False, XtNallowResize, True, nullptr);
        if (!state->first || !state->second) {
            self->destroy();
            throw std::runtime_error(
                "X11/Athena: split_view panes have no native widgets.");
        }
        _content_hosts_are_panes = true;
        self->refresh_contents();
        XtAddEventHandler(state->first, StructureNotifyMask, False,
            pane_resized, &get_first());
        XtAddEventHandler(state->second, StructureNotifyMask, False,
            pane_resized, &get_second());
        XtVaSetValues(state->first,
                      XtNallowResize,
                      True,
                      XtNshowGrip,
                      False,
                      nullptr);
        XtVaSetValues(state->second,
                      XtNallowResize,
                      True,
                      XtNshowGrip,
                      False,
                      nullptr);
        XtAddEventHandler(state->paned,
                          ExposureMask | StructureNotifyMask | ButtonPressMask |
                              ButtonReleaseMask | PointerMotionMask,
                          False,
                          separator_event,
                          self);
        self->apply_minimums();
        self->apply_ratio();
    }

    void split_view::show_native() {
        auto *state = binding(*this);
        if (!_created || !state || !state->paned)
            throw std::runtime_error(
                "X11/Athena: split_view is not created.");
        XtManageChild(state->paned);
        get_first().show();
        get_second().show();
    }

    void split_view::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *state = binding(*self);
        if (state && state->paned) {
            XtUngrabPointer(state->paned, CurrentTime);
            linux::x11::wnd_bindings.unregister_by_handle(state->paned);
            XtDestroyWidget(state->paned);
        }
        linux::x11::split_view_bindings.unregister_by_handle(self);
        delete state;
    }
} // namespace native
