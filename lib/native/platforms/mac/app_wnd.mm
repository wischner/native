//
// Implements the macOS application-window backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <utility>

#include <AppKit/AppKit.h>

#include <native.h>
#include <native/app_wnd.h>

#include "native_window.h"
#include "globals.h"
#include "../../window_icon.h"

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

    void app_wnd::apply_icon_text() {
        NSWindow *window = mac::wnd_bindings.handle_from_object(this);
        if (!window)
            throw std::runtime_error(
                "macOS: Missing NSWindow binding for app_wnd.");

        // The Dock labels a minimized window with its miniwindow title.
        [window setMiniwindowTitle:
            [NSString stringWithUTF8String:
                get_effective_icon_text().c_str()]];
    }

    void app_wnd::apply_icon() {
        NSWindow *window = mac::wnd_bindings.handle_from_object(this);
        if (!window)
            throw std::runtime_error(
                "macOS: Missing NSWindow binding for app_wnd.");

        // AppKit windows have no icon of their own. The main window
        // supplies the Dock image; other windows only cache theirs.
        if (this != app::main_wnd())
            return;
        NSImage *image = nil;
        if (_icon) {
            static_assert(sizeof(rgba) == 4);
            const int edge = std::clamp<int>(
                std::max(_icon->w(), _icon->h()), 16, 1024);
            const auto square = detail::fit_window_icon(
                *_icon, static_cast<dim>(edge));
            NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc]
                initWithBitmapDataPlanes:nullptr
                              pixelsWide:edge
                              pixelsHigh:edge
                           bitsPerSample:8
                         samplesPerPixel:4
                                hasAlpha:YES
                                isPlanar:NO
                          colorSpaceName:NSDeviceRGBColorSpace
                            bitmapFormat:NSBitmapFormatAlphaNonpremultiplied
                             bytesPerRow:edge * 4
                            bitsPerPixel:32];
            if (!bitmap)
                throw std::runtime_error(
                    "macOS: Failed to create the application icon.");
            std::memcpy([bitmap bitmapData], square->pixels(),
                        std::size_t(edge) * edge * 4);
            image = [[NSImage alloc]
                initWithSize:NSMakeSize(edge, edge)];
            [image addRepresentation:bitmap];
            [bitmap release];
        }
        // Nil restores the bundle or generic application icon.
        [[NSApplication sharedApplication]
            setApplicationIconImage:image];
        [image release];
    }

    void app_wnd::create_native() {
        validate_owner_created();
        const rect b = get_bounds();
        [[maybe_unused]] mac::native_window native_window(
            this,
            _title.c_str(),
            b.p.x,
            b.p.y,
            static_cast<int>(b.d.w),
            static_cast<int>(b.d.h));

        if (!mac::wnd_bindings.handle_from_object(
                this))
            throw std::runtime_error(
                "macOS: failed to create native window.");

        this->menu.attach(
            *this);
        apply_icon_text();
        if (_icon)
            apply_icon();
    }

    void app_wnd::show_native() {
        if (!_created)
            throw std::runtime_error(
                "macOS: Cannot show window before it is created.");

        NSWindow *win = mac::wnd_bindings.handle_from_object(
            this);
        if (!win)
            throw std::runtime_error(
                "macOS: Missing NSWindow binding for app_wnd.");

        if (app_wnd *owner = get_modal() ? get_owner() : nullptr) {
            NSWindow *owner_window =
                mac::wnd_bindings.handle_from_object(owner);
            if (owner_window && [win sheetParent] != owner_window)
                [owner_window beginSheet:win
                       completionHandler:nil];
        } else {
            if ([win sheetParent])
                [[win sheetParent] endSheet:win];
            [win setLevel:NSNormalWindowLevel];
        }

        [win makeKeyAndOrderFront:nil];
        if (mac::global_app)
            [mac::global_app activateIgnoringOtherApps:YES];
        invalidate();
    }

    void app_wnd::destroy_native() {
        if (!_created)
            return;

        auto *self = this;
        NSWindow *win = mac::wnd_bindings.handle_from_object(self);
        app_wnd *owner = get_owner();
        NSWindow *owner_window =
            owner ? mac::wnd_bindings.handle_from_object(owner) : nil;
        id delegate = mac::take_window_delegate(*self);

        if (win) {
            if ([win sheetParent])
                [[win sheetParent] endSheet:win];
            else if ([win parentWindow])
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
