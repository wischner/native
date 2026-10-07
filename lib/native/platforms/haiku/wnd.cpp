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
#include <Region.h>
#include <TextView.h>

#include <native.h>
#include <native/wnd.h>

#include "gpx_wnd.h"
#include "globals.h"
#include "../../mouse_state.h"

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
    void wnd::apply_border_sides() {
        // Buttons draw their complete background and chosen edges in Draw.
        // A persistent native clip would also prevent erasing old edges.
        if (dynamic_cast<button *>(this)) return;
        if (!(dynamic_cast<button *>(this) || dynamic_cast<text_edit *>(this) ||
              dynamic_cast<combo_box *>(this) || dynamic_cast<list *>(this) ||
              dynamic_cast<tree_view *>(this) || dynamic_cast<table_view *>(this) ||
              dynamic_cast<tab_view *>(this) || dynamic_cast<icon_view *>(this) ||
              dynamic_cast<accordion *>(this))) return;
        if (BView *view = haiku::view_from_control(this)) {
            // A plain BTextView has no outer frame to mask.
            if (dynamic_cast<BTextView *>(view)) return;
            with_locked_window(view->Window(), [&](BWindow *) {
                const auto sides = get_border_sides();
                view->ConstrainClippingRegion(nullptr);
                if (sides != border_sides::all) {
                    BRect bounds = view->Bounds();
                    if (!has_border(sides, border_sides::left)) bounds.left += 2;
                    if (!has_border(sides, border_sides::top)) bounds.top += 2;
                    if (!has_border(sides, border_sides::right)) bounds.right -= 2;
                    if (!has_border(sides, border_sides::bottom)) bounds.bottom -= 2;
                    BRegion region(bounds);
                    view->ConstrainClippingRegion(&region);
                }
                view->Invalidate();
            });
        }
    }

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

        // The peer releases cursors before the app_server connection closes.
        struct cursor_state {
            std::unique_ptr<BCursor> value;
            mouse_cursor policy = mouse_cursor::arrow;
        };
        auto *state = detail::peer_state<cursor_state>(*this);
        if (!state) {
            state = new cursor_state();
            detail::assign_peer_state(*this, state);
        }
        std::unique_ptr<BCursor> replacement;
        if (state->policy != _cursor) {
            constexpr unsigned char hidden_data[68] = {16, 1, 0, 0};
            switch (_cursor) {
            case mouse_cursor::hidden:
                replacement = std::make_unique<BCursor>(hidden_data);
                break;
            case mouse_cursor::crosshair:
                replacement = std::make_unique<BCursor>(B_CURSOR_ID_CROSS_HAIR);
                break;
            case mouse_cursor::resize_horizontal:
                replacement = std::make_unique<BCursor>(B_CURSOR_ID_RESIZE_EAST_WEST);
                break;
            case mouse_cursor::resize_vertical:
                replacement = std::make_unique<BCursor>(B_CURSOR_ID_RESIZE_NORTH_SOUTH);
                break;
            case mouse_cursor::resize_northwest_southeast:
                replacement = std::make_unique<BCursor>(B_CURSOR_ID_RESIZE_NORTH_WEST_SOUTH_EAST);
                break;
            case mouse_cursor::resize_northeast_southwest:
                replacement = std::make_unique<BCursor>(B_CURSOR_ID_RESIZE_NORTH_EAST_SOUTH_WEST);
                break;
            default: break;
            }
        }
        const BCursor *cursor = B_CURSOR_SYSTEM_DEFAULT;
        if (_cursor == mouse_cursor::ibeam) cursor = B_CURSOR_I_BEAM;
        else if (replacement) cursor = replacement.get();
        else if (state->policy == _cursor && state->value) cursor = state->value.get();

        with_locked_window(window, [&](BWindow *) {
            BPoint position;
            uint32 buttons = 0;
            view->GetMouse(&position, &buttons, false);
            if (!window->IsActive() || !get_client_bounds().contains(point(
                    static_cast<coord>(position.x), static_cast<coord>(position.y))))
                cursor = B_CURSOR_SYSTEM_DEFAULT;
            view->SetViewCursor(cursor, true);
            if (state->policy != _cursor) {
                state->value = std::move(replacement);
                state->policy = _cursor;
            }
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

namespace native::detail
{
    mouse_capabilities backend_mouse_capabilities() { return {true, false, false, false}; }
    bool backend_capture_mouse(wnd &, mouse_capture_options, std::string &error) {
        error = "Explicit pointer capture is unavailable on this backend.";
        return false;
    }
    void backend_release_mouse(wnd &owner, mouse_capture_mode) { backend_refresh_mouse(owner); }
    void backend_refresh_mouse(wnd &owner) { mouse_access::refresh(owner); }
}
