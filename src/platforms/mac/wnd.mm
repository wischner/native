#include <native.h>
#include <bindings.h>
#include <AppKit/AppKit.h>
#include <algorithm>

#include "gpx_wnd.h"
#include "globals.h"

namespace mac
{
    extern native::bindings<NSWindow *, native::wnd *> wnd_bindings;
    extern native::bindings<native::wnd *, macgpx *> wnd_gpx_bindings;
}

namespace
{
    native::rect layout_bounds_for(native::wnd *w)
    {
        native::size d = w->dimensions();
        return native::rect(0, 0, d.w, d.h);
    }
}

namespace native
{
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
        if (_parent)
        {
            _parent->_children.erase(
                std::remove(_parent->_children.begin(), _parent->_children.end(), this),
                _parent->_children.end());
            if (_parent->_layout)
                _parent->_layout->remove_child(this);
        }

        if (_created)
        {
            // Clean up graphics cache
            if (auto *cache = mac::wnd_gpx_bindings.from_a(this))
            {
                // Note: NSView is owned by NSWindow, don't release it
                delete cache;
                mac::wnd_gpx_bindings.unregister_by_a(this);
            }

            mac::wnd_bindings.unregister_by_b(this);
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
            NSWindow *nswin = mac::wnd_bindings.from_b(this);
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
            NSWindow *nswin = mac::wnd_bindings.from_b(this);
            if (nswin)
            {
                NSSize sz = NSMakeSize(s.w, s.h);
                [nswin setContentSize:sz];
            }
        }

        if (_layout)
            _layout->relayout(this, layout_bounds_for(this));

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
            NSWindow *nswin = mac::wnd_bindings.from_b(this);
            if (nswin)
            {
                NSRect frame = NSMakeRect(r.p.x, r.p.y, r.d.w, r.d.h);
                [nswin setFrame:frame display:YES];
            }
        }

        if (_layout)
            _layout->relayout(this, layout_bounds_for(this));

        return *this;
    }

    void wnd::on_native_resize(const size &s)
    {
        _bounds.d = s;
        if (_layout)
            _layout->relayout(this, layout_bounds_for(this));
    }

    void wnd::set_layout(std::unique_ptr<layout_manager> layout)
    {
        _layout = std::move(layout);
        if (_layout)
        {
            for (auto *child : _children)
                _layout->add_child(child);
            _layout->relayout(this, layout_bounds_for(this));
        }
    }

    layout_manager *wnd::layout() const
    {
        return _layout.get();
    }

    wnd &wnd::set_parent(wnd *p)
    {
        if (_parent == p)
            return *this;

        wnd *old_parent = _parent;
        if (old_parent)
        {
            old_parent->_children.erase(
                std::remove(old_parent->_children.begin(), old_parent->_children.end(), this),
                old_parent->_children.end());

            if (old_parent->_layout)
            {
                old_parent->_layout->remove_child(this);
                old_parent->_layout->relayout(old_parent, layout_bounds_for(old_parent));
            }
        }

        _parent = p;

        if (_parent)
        {
            if (std::find(_parent->_children.begin(), _parent->_children.end(), this) == _parent->_children.end())
                _parent->_children.push_back(this);

            if (_parent->_layout)
            {
                _parent->_layout->add_child(this);
                _parent->_layout->relayout(_parent, layout_bounds_for(_parent));
            }
        }

        if (_created && p && p->_created)
        {
            NSWindow *child = mac::wnd_bindings.from_b(this);
            NSWindow *parent = mac::wnd_bindings.from_b(p);
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

        NSWindow *nswin = mac::wnd_bindings.from_b(const_cast<wnd *>(this));
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

        NSWindow *nswin = mac::wnd_bindings.from_b(const_cast<wnd *>(this));
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
