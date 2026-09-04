//
// Implements the Haiku window backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>
#include <AppDefs.h>
#include <Button.h>
#include <Cursor.h>
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

    void resize_portable_tab_pages(native::wnd &owner) {
        auto *tabs = dynamic_cast<native::tab_view *>(&owner);
        if (!tabs)
            return;
        auto *binding = haiku::tab_view_bindings.object_from_handle(tabs);
        if (!binding || binding->tabs)
            return;
        const native::rect content = tabs->get_content_bounds();
        for (BView *page : binding->pages) {
            page->MoveTo(content.p.x, content.p.y);
            page->ResizeTo(
                std::max(0, static_cast<int>(content.d.w) - 1),
                std::max(0, static_cast<int>(content.d.h) - 1));
        }
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
                resize_portable_tab_pages(*this);
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
                resize_portable_tab_pages(*this);
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

    void wnd::apply_cursor() {
        BView *view = haiku::view_from_control(this);
        BWindow *window = view
                              ? view->Window()
                              : haiku::wnd_bindings
                                    .handle_from_object(this);
        if (!view && window)
            view = window->ChildAt(0);
        if (!view)
            return;

        static const BCursor crosshair(B_CURSOR_ID_CROSS_HAIR);
        static const BCursor horizontal_resize(
            B_CURSOR_ID_RESIZE_EAST_WEST);
        static const BCursor vertical_resize(
            B_CURSOR_ID_RESIZE_NORTH_SOUTH);
        static const BCursor northwest_southeast_resize(
            B_CURSOR_ID_RESIZE_NORTH_WEST_SOUTH_EAST);
        static const BCursor northeast_southwest_resize(
            B_CURSOR_ID_RESIZE_NORTH_EAST_SOUTH_WEST);
        const BCursor *cursor = B_CURSOR_SYSTEM_DEFAULT;
        if (_cursor == mouse_cursor::ibeam)
            cursor = B_CURSOR_I_BEAM;
        else if (_cursor == mouse_cursor::crosshair)
            cursor = &crosshair;
        else if (_cursor == mouse_cursor::resize_horizontal)
            cursor = &horizontal_resize;
        else if (_cursor == mouse_cursor::resize_vertical)
            cursor = &vertical_resize;
        else if (_cursor == mouse_cursor::resize_northwest_southeast)
            cursor = &northwest_southeast_resize;
        else if (_cursor == mouse_cursor::resize_northeast_southwest)
            cursor = &northeast_southwest_resize;

        with_locked_window(window, [&](BWindow *) {
            view->SetViewCursor(cursor);
        });
    }

    wnd &wnd::invalidate_native() {
        if (!_created)
            return *this;

        if (BView *control =
                haiku::view_from_control(this)) {
            with_locked_window(control->Window(), [&](BWindow *) {
                control->Invalidate();
            });
            return *this;
        }

        BWindow *bwin = haiku::wnd_bindings.handle_from_object(
            this);
        with_locked_window(bwin, [](BWindow *window) {
            BView *view = window->ChildAt(0);
            if (view)
                view->Invalidate();
        });

        return *this;
    }

    wnd &wnd::invalidate_native(const rect &r) {
        if (!_created)
            return *this;

        if (BView *control =
                haiku::view_from_control(this)) {
            with_locked_window(control->Window(), [&](BWindow *) {
                control->Invalidate(
                    BRect(r.p.x, r.p.y, r.x2() - 1, r.y2() - 1));
            });
            return *this;
        }

        BWindow *bwin = haiku::wnd_bindings.handle_from_object(
            this);
        with_locked_window(bwin, [&](BWindow *window) {
            BView *view = window->ChildAt(0);
            if (!view)
                return;

            BRect rect(r.p.x, r.p.y, r.x2() - 1, r.y2() - 1);
            view->Invalidate(rect);
        });

        return *this;
    }

    gpx &wnd::get_gpx() {
        if (!_created)
            throw std::runtime_error(
                "Cannot obtain gpx before window is created.");

        if (!_gpx)
            _gpx = new gpx_wnd(this);

        return *_gpx;
    }

} // namespace native
