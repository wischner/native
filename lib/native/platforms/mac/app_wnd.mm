//
// Implements the macOS application-window backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>
#include <utility>

#include <AppKit/AppKit.h>

#include <native.h>
#include <native/app_wnd.h>

#include "native_window.h"
#include "globals.h"

namespace native
{
    void app_wnd::apply_title() {
        NSWindow *window = mac::wnd_bindings.handle_from_object(this);
        if (!window)
            throw std::runtime_error(
                "macOS: Missing NSWindow binding for app_wnd.");

        [window
            setTitle:[NSString stringWithUTF8String:_title.c_str()]];
    }

    void app_wnd::create() const {
        if (_created)
            return;

        validate_owner_created();
        const rect b = get_bounds();
        [[maybe_unused]] mac::native_window native_window(
            const_cast<app_wnd *>(this),
            _title.c_str(),
            b.p.x,
            b.p.y,
            static_cast<int>(b.d.w),
            static_cast<int>(b.d.h));

        if (!mac::wnd_bindings.handle_from_object(
                const_cast<app_wnd *>(this)))
            throw std::runtime_error(
                "macOS: failed to create native window.");

        _created = true;
        const_cast<app_wnd *>(this)->menu.attach(
            *const_cast<app_wnd *>(this));
        const_cast<app_wnd *>(this)->on_wnd_create.emit();
    }

    void app_wnd::show() const {
        if (!_created)
            throw std::runtime_error(
                "macOS: Cannot show window before it is created.");

        NSWindow *win = mac::wnd_bindings.handle_from_object(
            const_cast<app_wnd *>(this));
        if (!win)
            throw std::runtime_error(
                "macOS: Missing NSWindow binding for app_wnd.");

        if (app_wnd *owner = get_owner()) {
            NSWindow *owner_window =
                mac::wnd_bindings.handle_from_object(owner);
            if (owner_window && [win parentWindow] != owner_window)
                [owner_window addChildWindow:win
                                      ordered:NSWindowAbove];
            if (get_modal())
                [win setLevel:NSModalPanelWindowLevel];
        }

        [win makeKeyAndOrderFront:nil];
        if (mac::global_app)
            [mac::global_app activateIgnoringOtherApps:YES];
        invalidate();
    }

    void app_wnd::destroy() const {
        if (!_created)
            return;

        auto *self = const_cast<app_wnd *>(this);
        NSWindow *win = mac::wnd_bindings.handle_from_object(self);
        app_wnd *owner = get_owner();
        NSWindow *owner_window =
            owner ? mac::wnd_bindings.handle_from_object(owner) : nil;
        id delegate = mac::delegate_bindings.object_from_handle(self);
        self->on_native_destroy();
        mac::delegate_bindings.unregister_by_handle(self);

        if (win) {
            if ([win parentWindow])
                [[win parentWindow] removeChildWindow:win];
            [win setDelegate:nil];
            mac::wnd_bindings.unregister_by_object(self);
            [win close];
            [win release];
        }
        if (delegate)
            [delegate release];

        if (get_modal() && owner && owner_window) {
            if (owner->get_input_enabled()) {
                [owner_window makeKeyAndOrderFront:nil];
            } else if (modal_wnd *active =
                           owner->get_active_modal()) {
                NSWindow *active_window =
                    mac::wnd_bindings.handle_from_object(active);
                if (active_window)
                    [active_window makeKeyAndOrderFront:nil];
            }
        }

        if (self == app::main_wnd() && mac::global_app)
            [mac::global_app stop:nil];
    }
} // namespace native
