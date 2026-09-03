//
// Implements the Haiku window backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>
#include <Button.h>
#include <Window.h>
#include <View.h>

#include <native.h>
#include <native/wnd.h>

#include "gpx_wnd.h"
#include "globals.h"

namespace
{
    template <typename function_type>
    void with_locked_window(BWindow *window, function_type &&function) {
        if (!window)
            return;

        const bool already_locked = window->IsLocked();
        if (!already_locked && !window->Lock())
            return;

        function(window);

        if (!already_locked)
            window->Unlock();
    }

    float native_extent(native::dim dimension) {
        return dimension > 0
                   ? static_cast<float>(dimension - 1)
                   : 0.0f;
    }

} // namespace

namespace native
{
    void wnd::apply_position() {
        if (BView *control = haiku::view_from_control(this)) {
            with_locked_window(control->Window(), [&](BWindow *) {
                control->MoveTo(_bounds.p.x, _bounds.p.y);
            });
            return;
        }

        BWindow *window = haiku::wnd_bindings.handle_from_object(this);
        with_locked_window(window, [&](BWindow *locked) {
            locked->MoveTo(_bounds.p.x, _bounds.p.y);
        });
    }

    void wnd::apply_dimensions() {
        if (BView *control = haiku::view_from_control(this)) {
            with_locked_window(control->Window(), [&](BWindow *) {
                control->ResizeTo(native_extent(_bounds.d.w),
                                  native_extent(_bounds.d.h));
            });
            return;
        }

        BWindow *window = haiku::wnd_bindings.handle_from_object(this);
        with_locked_window(window, [&](BWindow *locked) {
            locked->ResizeTo(native_extent(_bounds.d.w),
                             native_extent(_bounds.d.h));
        });
    }

    void wnd::apply_bounds() {
        if (BView *control = haiku::view_from_control(this)) {
            with_locked_window(control->Window(), [&](BWindow *) {
                control->MoveTo(_bounds.p.x, _bounds.p.y);
                control->ResizeTo(native_extent(_bounds.d.w),
                                  native_extent(_bounds.d.h));
            });
            return;
        }

        BWindow *window = haiku::wnd_bindings.handle_from_object(this);
        with_locked_window(window, [&](BWindow *locked) {
            locked->MoveTo(_bounds.p.x, _bounds.p.y);
            locked->ResizeTo(native_extent(_bounds.d.w),
                             native_extent(_bounds.d.h));
        });
    }

    void wnd::apply_parent() {
        BView *control = haiku::view_from_control(this);
        if (!control)
            return;

        BWindow *old_window = control->Window();
        if (old_window) {
            with_locked_window(old_window, [&](BWindow *) {
                control->RemoveSelf();
            });
        }

        BView *new_parent = haiku::parent_view(_parent, this);
        BWindow *new_window = new_parent ? new_parent->Window() : nullptr;
        if (new_window) {
            with_locked_window(new_window, [&](BWindow *) {
                new_parent->AddChild(control);
            });
        }
    }

    wnd &wnd::invalidate() const {
        if (!_created)
            return const_cast<wnd &>(*this);

        if (BView *control =
                haiku::view_from_control(const_cast<wnd *>(this))) {
            with_locked_window(control->Window(), [&](BWindow *) {
                control->Invalidate();
            });
            return const_cast<wnd &>(*this);
        }

        BWindow *bwin = haiku::wnd_bindings.handle_from_object(
            const_cast<wnd *>(this));
        with_locked_window(bwin, [](BWindow *window) {
            BView *view = window->ChildAt(0);
            if (view)
                view->Invalidate();
        });

        return const_cast<wnd &>(*this);
    }

    wnd &wnd::invalidate(const rect &r) const {
        if (!_created)
            return const_cast<wnd &>(*this);

        if (BView *control =
                haiku::view_from_control(const_cast<wnd *>(this))) {
            with_locked_window(control->Window(), [&](BWindow *) {
                control->Invalidate(
                    BRect(r.p.x, r.p.y, r.x2() - 1, r.y2() - 1));
            });
            return const_cast<wnd &>(*this);
        }

        BWindow *bwin = haiku::wnd_bindings.handle_from_object(
            const_cast<wnd *>(this));
        with_locked_window(bwin, [&](BWindow *window) {
            BView *view = window->ChildAt(0);
            if (!view)
                return;

            BRect rect(r.p.x, r.p.y, r.x2() - 1, r.y2() - 1);
            view->Invalidate(rect);
        });

        return const_cast<wnd &>(*this);
    }

    gpx &wnd::get_gpx() const {
        if (!_created)
            throw std::runtime_error(
                "Cannot obtain gpx before window is created.");

        if (!_gpx)
            _gpx = new gpx_wnd(this);

        return *_gpx;
    }

} // namespace native
