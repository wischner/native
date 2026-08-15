//
// Implements the macOS window backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>
#include <bindings.h>
#include <AppKit/AppKit.h>

#include "gpx_wnd.h"
#include "globals.h"

namespace native
{
    void wnd::apply_position() {
        if (auto *control = dynamic_cast<button *>(this)) {
            auto *binding = mac::button_bindings.object_from_handle(control);
            if (binding && binding->ns_button) {
                [binding->ns_button setFrameOrigin:NSMakePoint(
                    _bounds.p.x, _bounds.p.y)];
            }
            return;
        }

        NSWindow *window = mac::wnd_bindings.handle_from_object(this);
        if (window) {
            [window setFrameOrigin:NSMakePoint(
                _bounds.p.x, _bounds.p.y)];
        }
    }

    void wnd::apply_dimensions() {
        if (auto *control = dynamic_cast<button *>(this)) {
            auto *binding = mac::button_bindings.object_from_handle(control);
            if (binding && binding->ns_button) {
                [binding->ns_button setFrameSize:NSMakeSize(
                    _bounds.d.w, _bounds.d.h)];
            }
            return;
        }

        NSWindow *window = mac::wnd_bindings.handle_from_object(this);
        if (window) {
            [window setContentSize:NSMakeSize(
                _bounds.d.w, _bounds.d.h)];
        }
    }

    void wnd::apply_bounds() {
        NSRect frame = NSMakeRect(
            _bounds.p.x, _bounds.p.y,
            _bounds.d.w, _bounds.d.h);

        if (auto *control = dynamic_cast<button *>(this)) {
            auto *binding = mac::button_bindings.object_from_handle(control);
            if (binding && binding->ns_button)
                [binding->ns_button setFrame:frame];
            return;
        }

        NSWindow *window = mac::wnd_bindings.handle_from_object(this);
        if (window)
            [window setFrame:frame display:YES];
    }

    void wnd::apply_parent() {
        if (auto *control = dynamic_cast<button *>(this)) {
            auto *binding = mac::button_bindings.object_from_handle(control);
            if (!binding || !binding->ns_button)
                return;

            [binding->ns_button removeFromSuperview];
            NSWindow *parent = _parent
                                   ? mac::wnd_bindings.handle_from_object(
                                         _parent)
                                   : nil;
            if (parent)
                [[parent contentView] addSubview:binding->ns_button];
            return;
        }

        NSWindow *child = mac::wnd_bindings.handle_from_object(this);
        if (!child)
            return;

        if ([child parentWindow])
            [[child parentWindow] removeChildWindow:child];

        NSWindow *parent = _parent
                               ? mac::wnd_bindings.handle_from_object(_parent)
                               : nil;
        if (parent)
            [parent addChildWindow:child ordered:NSWindowAbove];
    }

    wnd &wnd::invalidate() const {
        if (!_created)
            return const_cast<wnd &>(*this);

        if (auto *control = dynamic_cast<const button *>(this)) {
            auto *binding = mac::button_bindings.object_from_handle(
                const_cast<button *>(control));
            if (binding && binding->ns_button)
                [binding->ns_button setNeedsDisplay:YES];
            return const_cast<wnd &>(*this);
        }

        NSWindow *nswin = mac::wnd_bindings.handle_from_object(
            const_cast<wnd *>(this));
        if (nswin) {
            [[nswin contentView] setNeedsDisplay:YES];
        }

        return const_cast<wnd &>(*this);
    }

    wnd &wnd::invalidate(const rect &r) const {
        if (!_created)
            return const_cast<wnd &>(*this);

        if (auto *control = dynamic_cast<const button *>(this)) {
            auto *binding = mac::button_bindings.object_from_handle(
                const_cast<button *>(control));
            if (binding && binding->ns_button) {
                [binding->ns_button setNeedsDisplayInRect:NSMakeRect(
                    r.p.x, r.p.y, r.d.w, r.d.h)];
            }
            return const_cast<wnd &>(*this);
        }

        NSWindow *nswin = mac::wnd_bindings.handle_from_object(
            const_cast<wnd *>(this));
        if (nswin) {
            NSRect rect = NSMakeRect(r.p.x, r.p.y, r.d.w, r.d.h);
            [[nswin contentView] setNeedsDisplayInRect:rect];
        }

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
