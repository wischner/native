#include <native.h>
#include <bindings.h>
#include <AppKit/AppKit.h>
#include <iostream>

#include "gpx_wnd.h"
#include "globals.h"

namespace mac
{
    extern native::bindings<NSWindow *, native::wnd *> wnd_bindings;
    extern native::bindings<native::wnd *, gnustepgpx *> wnd_gpx_bindings;
}

namespace native
{

    void app_wnd::create() const
    {
        NSRect frame = NSMakeRect(x(), y(), width(), height());
        NSUInteger style = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable;

        NSWindow *window = [[NSWindow alloc] initWithContentRect:frame
                                                       styleMask:style
                                                         backing:NSBackingStoreBuffered
                                                           defer:NO];

        if (!window)
        {
            std::cerr << "GNUstep: Failed to create NSWindow.\n";
            return;
        }

        [window setTitle:[NSString stringWithUTF8String:title().c_str()]];
        gnustep::wnd_bindings.register_pair(window, const_cast<app_wnd *>(this));
    }

    void app_wnd::show() const
    {
        NSWindow *window = gnustep::wnd_bindings.from_b(const_cast<app_wnd *>(this));
        if (!window)
        {
            std::cerr << "GNUstep: Cannot show window — not created.\n";
            return;
        }

        [window makeKeyAndOrderFront:nil];
    }

    wnd::wnd(coord x, coord y, dim w, dim h)
        : _parent(nullptr), _created(false)
    {
        set_bounds({{x, y}, {w, h}});
    }

    wnd::wnd(const point &pos, const size &dim)
        : _parent(nullptr), _created(false)
    {
        set_bounds({pos, dim});
    }

    wnd::wnd(const rect &bounds)
        : _parent(nullptr), _created(false)
    {
        set_bounds(bounds);
    }

    wnd::~wnd()
    {
        if (_created)
        {
            // Clean up graphics cache
            if (auto *cache = gnustep::wnd_gpx_bindings.from_a(this))
            {
                // Note: NSView is owned by NSWindow, don't release it
                delete cache;
                gnustep::wnd_gpx_bindings.unregister_by_a(this);
            }

            gnustep::wnd_bindings.unregister_by_b(this);
        }
    }

    point wnd::position() const
    {
        return _bounds.p;
    }

    wnd &wnd::set_position(const point &p)
    {
        _bounds.p = p;

        if (_created)
        {
            NSWindow *nswin = gnustep::wnd_bindings.from_b(this);
            if (nswin)
            {
                NSPoint origin = NSMakePoint(p.x, p.y);
                [nswin setFrameOrigin:origin];
            }
        }

        return *this;
    }

    size wnd::dimensions() const
    {
        return _bounds.d;
    }

    wnd &wnd::set_dimensions(const size &s)
    {
        _bounds.d = s;

        if (_created)
        {
            NSWindow *nswin = gnustep::wnd_bindings.from_b(this);
            if (nswin)
            {
                NSSize sz = NSMakeSize(s.w, s.h);
                [nswin setContentSize:sz];
            }
        }

        return *this;
    }

    rect wnd::bounds() const
    {
        return _bounds;
    }

    wnd &wnd::set_bounds(const rect &r)
    {
        _bounds = r;

        if (_created)
        {
            NSWindow *nswin = gnustep::wnd_bindings.from_b(this);
            if (nswin)
            {
                NSRect frame = NSMakeRect(r.p.x, r.p.y, r.d.w, r.d.h);
                [nswin setFrame:frame display:YES];
            }
        }

        return *this;
    }

    void wnd::set_layout(std::unique_ptr<layout_manager> layout)
    {
        _layout = std::move(layout);
        if (_layout && _created)
            _layout->relayout(this, _bounds);
    }

    layout_manager *wnd::layout() const
    {
        return _layout.get();
    }

    wnd &wnd::set_parent(wnd *p)
    {
        _parent = p;

        if (_created && p && p->_created)
        {
            NSWindow *child = gnustep::wnd_bindings.from_b(this);
            NSWindow *parent = gnustep::wnd_bindings.from_b(p);
            if (child && parent)
            {
                [parent addChildWindow:child ordered:NSWindowAbove];
            }
        }

        return *this;
    }

    wnd *wnd::parent() const
    {
        return _parent;
    }

    wnd &wnd::invalidate() const
    {
        if (!_created)
            return const_cast<wnd &>(*this);

        NSWindow *nswin = gnustep::wnd_bindings.from_b(const_cast<wnd *>(this));
        if (nswin)
        {
            [[nswin contentView] setNeedsDisplay:YES];
        }

        return const_cast<wnd &>(*this);
    }

    wnd &wnd::invalidate(const rect &r) const
    {
        if (!_created)
            return const_cast<wnd &>(*this);

        NSWindow *nswin = gnustep::wnd_bindings.from_b(const_cast<wnd *>(this));
        if (nswin)
        {
            NSRect rect = NSMakeRect(r.p.x, r.p.y, r.d.w, r.d.h);
            [[nswin contentView] setNeedsDisplayInRect:rect];
        }

        return const_cast<wnd &>(*this);
    }

    gpx &wnd::get_gpx() const
    {
        if (!_created)
            throw std::runtime_error("Cannot obtain gpx before window is created.");

        if (!_gpx)
            _gpx = new gpx_wnd(this);

        return *_gpx;
    }

} // namespace native
