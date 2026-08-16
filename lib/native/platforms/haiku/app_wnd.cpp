//
// Implements the Haiku application-window backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>
#include <utility>

#include <Application.h>
#include <Window.h>

#include <native.h>
#include <native/app_wnd.h>

#include "native_window.h"
#include "globals.h"

namespace native
{
    void app_wnd::apply_title() {
        BWindow *window = haiku::wnd_bindings.handle_from_object(this);
        if (!window)
            throw std::runtime_error(
                "Haiku: Missing BWindow binding for app_wnd.");
        if (!window->Lock())
            throw std::runtime_error(
                "Haiku: Failed to lock app_wnd while setting title.");

        window->SetTitle(_title.c_str());
        window->Unlock();
    }

    void app_wnd::create() const {
        if (_created)
            return;

        validate_owner_created();
        if (!haiku::global_app)
            haiku::global_app =
                new BApplication("application/x-vnd.wischner-native");

        BRect frame(static_cast<float>(_bounds.p.x),
                    static_cast<float>(_bounds.p.y),
                    static_cast<float>(_bounds.p.x + _bounds.d.w - 1),
                    static_cast<float>(_bounds.p.y + _bounds.d.h - 1));

        const window_look look =
            get_modal() ? B_MODAL_WINDOW_LOOK
                        : get_owner() ? B_FLOATING_WINDOW_LOOK
                                      : B_TITLED_WINDOW_LOOK;
        const window_feel feel =
            get_modal() ? B_MODAL_SUBSET_WINDOW_FEEL
                        : get_owner() ? B_FLOATING_SUBSET_WINDOW_FEEL
                                      : B_NORMAL_WINDOW_FEEL;
        auto *window = new haiku::native_window(
            const_cast<app_wnd *>(this),
            frame,
            _title.c_str(),
            look,
            feel);

        if (app_wnd *owner = get_owner()) {
            BWindow *owner_window =
                haiku::wnd_bindings.handle_from_object(owner);
            if (owner_window)
                window->AddToSubset(owner_window);
        }

        if (!haiku::wnd_bindings.handle_from_object(
                const_cast<app_wnd *>(this)))
            throw std::runtime_error(
                "Haiku: failed to create native window.");

        _created = true;
        const_cast<app_wnd *>(this)->menu.attach(
            *const_cast<app_wnd *>(this));
        const_cast<app_wnd *>(this)->on_wnd_create.emit();
    }

    void app_wnd::show() const {
        if (!_created)
            throw std::runtime_error(
                "Haiku: Cannot show window before it is created.");

        BWindow *win = haiku::wnd_bindings.handle_from_object(
            const_cast<app_wnd *>(this));
        if (!win)
            throw std::runtime_error(
                "Haiku: Missing BWindow binding for app_wnd.");

        win->Show();
        if (get_modal())
            win->Activate(true);
        invalidate();
    }

    void app_wnd::destroy() const {
        if (!_created)
            return;

        app_wnd *self = const_cast<app_wnd *>(this);
        BWindow *win = haiku::wnd_bindings.handle_from_object(self);
        app_wnd *owner = get_owner();
        BWindow *owner_window =
            owner ? haiku::wnd_bindings.handle_from_object(owner)
                  : nullptr;
        self->on_native_destroy();

        if (win) {
            haiku::wnd_bindings.unregister_by_object(self);
            if (win->Lock())
                win->Quit();
            else
                win->PostMessage(B_QUIT_REQUESTED);
        }

        if (get_modal() && owner && owner_window) {
            if (owner->get_input_enabled()) {
                owner_window->Activate(true);
            } else if (modal_wnd *active =
                           owner->get_active_modal()) {
                BWindow *active_window =
                    haiku::wnd_bindings.handle_from_object(active);
                if (active_window)
                    active_window->Activate(true);
            }
        }
    }
} // namespace native
