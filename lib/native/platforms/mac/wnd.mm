//
// Implements the macOS window backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>
#include <native/wnd.h>
#include <bindings.h>
#include <AppKit/AppKit.h>

#include "gpx_wnd.h"
#include "globals.h"

namespace native
{
    void wnd::apply_position() {
        if (NSView *control = mac::view_from_control(this)) {
            [control
                setFrameOrigin:NSMakePoint(_bounds.p.x, _bounds.p.y)];
            return;
        }

        NSWindow *window = mac::wnd_bindings.handle_from_object(this);
        if (window) {
            [window
                setFrameOrigin:NSMakePoint(_bounds.p.x, _bounds.p.y)];
        }
    }

    void wnd::apply_dimensions() {
        if (NSView *control = mac::view_from_control(this)) {
            [control setFrameSize:NSMakeSize(_bounds.d.w, _bounds.d.h)];
            return;
        }

        NSWindow *window = mac::wnd_bindings.handle_from_object(this);
        if (window) {
            [window
                setContentSize:NSMakeSize(_bounds.d.w, _bounds.d.h)];
        }
    }

    void wnd::apply_bounds() {
        NSRect frame = NSMakeRect(
            _bounds.p.x, _bounds.p.y, _bounds.d.w, _bounds.d.h);

        if (NSView *control = mac::view_from_control(this)) {
            [control setFrame:frame];
            return;
        }

        NSWindow *window = mac::wnd_bindings.handle_from_object(this);
        if (window)
            [window setFrame:frame display:YES];
    }

    void wnd::apply_parent() {
        if (NSView *control = mac::view_from_control(this)) {
            [control removeFromSuperview];
            if (NSView *parent = mac::parent_view(_parent))
                [parent addSubview:control];
            return;
        }

        NSWindow *child = mac::wnd_bindings.handle_from_object(this);
        if (!child)
            return;

        if ([child parentWindow])
            [[child parentWindow] removeChildWindow:child];

        NSWindow *parent =
            _parent ? mac::wnd_bindings.handle_from_object(_parent)
                    : nil;
        if (parent)
            [parent addChildWindow:child ordered:NSWindowAbove];
    }

    wnd &wnd::invalidate() const {
        if (!_created)
            return const_cast<wnd &>(*this);

        if (NSView *control =
                mac::view_from_control(const_cast<wnd *>(this))) {
            [control setNeedsDisplay:YES];
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

        if (NSView *control =
                mac::view_from_control(const_cast<wnd *>(this))) {
            [control
                setNeedsDisplayInRect:NSMakeRect(
                                          r.p.x, r.p.y, r.d.w, r.d.h)];
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
            throw std::runtime_error(
                "Cannot obtain gpx before window is created.");

        if (!_gpx)
            _gpx = new gpx_wnd(this);

        return *_gpx;
    }

} // namespace native
