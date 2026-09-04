//
// Implements the X11 paintable child surface. The shared Athena host
// owns the drawable, its back buffer, and the expose, structure, and
// pointer routing; the portable canvas owns everything drawn into it.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <X11/Intrinsic.h>

#include <native.h>
#include <native/canvas.h>

#include "collection_host.h"
#include "globals.h"

namespace native
{
    void canvas::create_native() {
        auto *self = this;
        Widget widget =
            linux::x11::create_collection_host(*self, "canvas");
        self->synchronize_theme_metrics();
        self->relayout_children();
        (void)widget;
    }

    void canvas::show_native() {
        Widget widget = linux::x11::wnd_bindings.handle_from_object(
            this);
        if (!_created || !widget)
            throw std::runtime_error(
                "X11/Athena: canvas is not created.");
        XtManageChild(widget);
        if (XtIsRealized(widget))
            XRaiseWindow(linux::x11::cached_display, XtWindow(widget));
    }

    void canvas::destroy_native() {
        if (!_created)
            return;

        auto *self = this;
        Widget widget =
            linux::x11::wnd_bindings.handle_from_object(self);
        if (widget) {
            linux::x11::wnd_bindings.unregister_by_handle(widget);
            XtDestroyWidget(widget);
        }
    }
} // namespace native
