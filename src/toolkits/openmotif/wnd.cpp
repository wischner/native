#include <stdexcept>

#include <Xm/Xm.h>
#include <X11/Xlib.h>

#include <native.h>
#include "bindings.h"
#include "gpx_wnd.h"
#include "globals.h"

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
        if (_created)
        {
            motif::wnd_bindings.unregister_by_b(this);
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
            Widget widget = motif::wnd_bindings.from_b(this);
            XtVaSetValues(widget,
                          XmNx, p.x,
                          XmNy, p.y,
                          NULL);
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
            Widget widget = motif::wnd_bindings.from_b(this);
            XtVaSetValues(widget,
                          XmNwidth, s.w,
                          XmNheight, s.h,
                          NULL);
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
            Widget widget = motif::wnd_bindings.from_b(this);
            XtVaSetValues(widget,
                          XmNx, r.p.x,
                          XmNy, r.p.y,
                          XmNwidth, r.d.w,
                          XmNheight, r.d.h,
                          NULL);
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
            Widget child = motif::wnd_bindings.from_b(this);
            Widget parent = motif::wnd_bindings.from_b(p);
            XtVaSetValues(child, XmNparent, parent, NULL);
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

        Widget widget = motif::wnd_bindings.from_b(const_cast<wnd *>(this));
        Window win = XtWindow(widget);
        XClearArea(motif::cached_display, win, 0, 0, 0, 0, True);
        XFlush(motif::cached_display);
        return const_cast<wnd &>(*this);
    }

    wnd &wnd::invalidate(const rect &r) const
    {
        if (!_created)
            return const_cast<wnd &>(*this);

        Widget widget = motif::wnd_bindings.from_b(const_cast<wnd *>(this));
        Window win = XtWindow(widget);
        XClearArea(motif::cached_display, win, r.p.x, r.p.y, r.d.w, r.d.h, True);
        XFlush(motif::cached_display);
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
