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
} // namespace

namespace native
{
    void wnd::apply_position() {
        if (auto *control = dynamic_cast<button *>(this)) {
            auto *binding =
                haiku::button_bindings.object_from_handle(control);
            if (binding && binding->button) {
                with_locked_window(
                    binding->button->Window(),
                    [&](BWindow *) {
                        binding->button->MoveTo(
                            _bounds.p.x, _bounds.p.y);
                    });
            }
            return;
        }

        BWindow *window = haiku::wnd_bindings.handle_from_object(this);
        with_locked_window(window, [&](BWindow *locked) {
            locked->MoveTo(_bounds.p.x, _bounds.p.y);
        });
    }

    void wnd::apply_dimensions() {
        if (auto *control = dynamic_cast<button *>(this)) {
            auto *binding =
                haiku::button_bindings.object_from_handle(control);
            if (binding && binding->button) {
                with_locked_window(
                    binding->button->Window(),
                    [&](BWindow *) {
                        binding->button->ResizeTo(
                            _bounds.d.w, _bounds.d.h);
                    });
            }
            return;
        }

        BWindow *window = haiku::wnd_bindings.handle_from_object(this);
        with_locked_window(window, [&](BWindow *locked) {
            locked->ResizeTo(_bounds.d.w, _bounds.d.h);
        });
    }

    void wnd::apply_bounds() {
        if (auto *control = dynamic_cast<button *>(this)) {
            auto *binding =
                haiku::button_bindings.object_from_handle(control);
            if (binding && binding->button) {
                with_locked_window(
                    binding->button->Window(),
                    [&](BWindow *) {
                        binding->button->MoveTo(
                            _bounds.p.x, _bounds.p.y);
                        binding->button->ResizeTo(
                            _bounds.d.w, _bounds.d.h);
                    });
            }
            return;
        }

        BWindow *window = haiku::wnd_bindings.handle_from_object(this);
        with_locked_window(window, [&](BWindow *locked) {
            locked->MoveTo(_bounds.p.x, _bounds.p.y);
            locked->ResizeTo(_bounds.d.w, _bounds.d.h);
        });
    }

    void wnd::apply_parent() {
        auto *control = dynamic_cast<button *>(this);
        if (!control)
            return;

        auto *binding = haiku::button_bindings.object_from_handle(control);
        if (!binding || !binding->button)
            return;

        BWindow *old_window = binding->button->Window();
        if (old_window) {
            with_locked_window(old_window, [&](BWindow *) {
                binding->button->RemoveSelf();
            });
        }

        BWindow *new_window = _parent
                                  ? haiku::wnd_bindings.handle_from_object(
                                        _parent)
                                  : nullptr;
        if (new_window) {
            with_locked_window(new_window, [&](BWindow *locked) {
                locked->AddChild(binding->button);
            });
        }
    }

    wnd &wnd::invalidate() const {
        if (!_created)
            return const_cast<wnd &>(*this);

        if (auto *control = dynamic_cast<const button *>(this)) {
            auto *binding = haiku::button_bindings.object_from_handle(
                const_cast<button *>(control));
            if (binding && binding->button) {
                with_locked_window(
                    binding->button->Window(),
                    [&](BWindow *) { binding->button->Invalidate(); });
            }
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

        if (auto *control = dynamic_cast<const button *>(this)) {
            auto *binding = haiku::button_bindings.object_from_handle(
                const_cast<button *>(control));
            if (binding && binding->button) {
                with_locked_window(
                    binding->button->Window(),
                    [&](BWindow *) {
                        binding->button->Invalidate(BRect(
                            r.p.x, r.p.y, r.x2() - 1, r.y2() - 1));
                    });
            }
            return const_cast<wnd &>(*this);
        }

        BWindow *bwin = haiku::wnd_bindings.handle_from_object(
            const_cast<wnd *>(this));
        with_locked_window(bwin, [&](BWindow *window) {
            BView *view = window->ChildAt(0);
            if (!view)
                return;

            BRect rect(
                r.p.x, r.p.y, r.x2() - 1, r.y2() - 1);
            view->Invalidate(rect);
        });

        return const_cast<wnd &>(*this);
    }

    gpx &wnd::get_gpx() const {
        if (!_created)
            throw std::runtime_error("Cannot obtain gpx before window is created.");

        if (!_gpx)
            _gpx = new gpx_wnd(this);

        return *_gpx;
    }

} // namespace native
