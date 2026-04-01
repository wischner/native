#include <stdexcept>
#include <algorithm>

#include <Xm/Xm.h>
#include <X11/Xlib.h>

#include <native.h>

#include "gpx_wnd.h"
#include "globals.h"

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
            motif::wnd_bindings.unregister_by_b(this);
            motif::shell_bindings.unregister_by_b(this);
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
            Widget shell = motif::shell_bindings.from_b(this);
            if (shell)
                XtVaSetValues(shell, XtNx, p.x, XtNy, p.y, nullptr);
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
            Widget shell = motif::shell_bindings.from_b(this);
            Widget canvas = motif::wnd_bindings.from_b(this);
            if (shell)
                XtVaSetValues(shell, XtNwidth, s.w, XtNheight, s.h, nullptr);
            if (canvas)
                XtVaSetValues(canvas, XmNwidth, s.w, XmNheight, s.h, nullptr);
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
            Widget shell = motif::shell_bindings.from_b(this);
            Widget canvas = motif::wnd_bindings.from_b(this);

            if (shell)
            {
                XtVaSetValues(
                    shell,
                    XtNx, r.p.x,
                    XtNy, r.p.y,
                    XtNwidth, r.d.w,
                    XtNheight, r.d.h,
                    nullptr);
            }

            if (canvas)
                XtVaSetValues(canvas, XmNwidth, r.d.w, XmNheight, r.d.h, nullptr);
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

        Widget canvas = motif::wnd_bindings.from_b(const_cast<wnd *>(this));
        if (canvas && XtIsRealized(canvas))
        {
            XClearArea(motif::cached_display, XtWindow(canvas), 0, 0, 0, 0, True);
            XFlush(motif::cached_display);
        }

        return const_cast<wnd &>(*this);
    }

    wnd &wnd::invalidate(const rect &r) const
    {
        if (!_created)
            return const_cast<wnd &>(*this);

        Widget canvas = motif::wnd_bindings.from_b(const_cast<wnd *>(this));
        if (canvas && XtIsRealized(canvas))
        {
            XClearArea(motif::cached_display, XtWindow(canvas), r.p.x, r.p.y, r.d.w, r.d.h, True);
            XFlush(motif::cached_display);
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
