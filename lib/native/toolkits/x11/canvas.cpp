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
    void canvas::create() const {
        if (_created)
            return;

        auto *self = const_cast<canvas *>(this);
        Widget widget =
            linux::x11::create_collection_host(*self, "canvas");
        _created = true;
        self->synchronize_theme_metrics();
        self->relayout_children();
        (void)widget;
        self->on_native_create();
    }

    void canvas::show() const {
        Widget widget = linux::x11::wnd_bindings.handle_from_object(
            const_cast<canvas *>(this));
        if (!_created || !widget)
            throw std::runtime_error(
                "X11/Athena: canvas is not created.");
        XtManageChild(widget);
        if (XtIsRealized(widget))
            XRaiseWindow(linux::x11::cached_display, XtWindow(widget));
    }

    void canvas::destroy() const {
        if (!_created)
            return;

        auto *self = const_cast<canvas *>(this);
        Widget widget =
            linux::x11::wnd_bindings.handle_from_object(self);
        self->on_native_destroy();
        if (widget) {
            linux::x11::wnd_bindings.unregister_by_handle(widget);
            XtDestroyWidget(widget);
        }
    }
} // namespace native
