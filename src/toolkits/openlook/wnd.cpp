#include <stdexcept>

#include <X11/Xlib.h>

#include <xview/xview.h>
#include <xview/window.h>
#ifdef coord
#undef coord
#endif

#include <native.h>

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
            openlook::canvas_bindings.unregister_by_b(this);
            openlook::frame_bindings.unregister_by_b(this);
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
            Xv_opaque frame = openlook::frame_bindings.from_b(this);
            if (frame)
                xv_set(frame, XV_X, p.x, XV_Y, p.y, 0);
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
            Xv_opaque frame = openlook::frame_bindings.from_b(this);
            Xv_opaque paint_window = openlook::canvas_bindings.from_b(this);
            if (frame)
                xv_set(frame, XV_WIDTH, s.w, XV_HEIGHT, s.h, 0);
            if (paint_window)
                xv_set(paint_window, XV_WIDTH, s.w, XV_HEIGHT, s.h, 0);
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
            Xv_opaque frame = openlook::frame_bindings.from_b(this);
            Xv_opaque paint_window = openlook::canvas_bindings.from_b(this);
            if (frame)
            {
                xv_set(frame,
                       XV_X, r.p.x,
                       XV_Y, r.p.y,
                       XV_WIDTH, r.d.w,
                       XV_HEIGHT, r.d.h,
                       0);
            }
            if (paint_window)
                xv_set(paint_window, XV_WIDTH, r.d.w, XV_HEIGHT, r.d.h, 0);
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

        Xv_opaque paint_window = openlook::canvas_bindings.from_b(const_cast<wnd *>(this));
        Display *display = paint_window ? reinterpret_cast<Display *>(xv_get(paint_window, XV_DISPLAY)) : nullptr;
        if (!display)
            display = openlook::cached_display;

        if (paint_window && display)
        {
            openlook::cached_display = display;
            const Window xwin = static_cast<Window>(xv_get(paint_window, XV_XID));
            if (xwin)
            {
                XClearArea(display, xwin, 0, 0, 0, 0, True);
                XFlush(display);
            }
        }

        return const_cast<wnd &>(*this);
    }

    wnd &wnd::invalidate(const rect &r) const
    {
        if (!_created)
            return const_cast<wnd &>(*this);

        Xv_opaque paint_window = openlook::canvas_bindings.from_b(const_cast<wnd *>(this));
        Display *display = paint_window ? reinterpret_cast<Display *>(xv_get(paint_window, XV_DISPLAY)) : nullptr;
        if (!display)
            display = openlook::cached_display;

        if (paint_window && display)
        {
            openlook::cached_display = display;
            const Window xwin = static_cast<Window>(xv_get(paint_window, XV_XID));
            if (xwin)
            {
                XClearArea(display, xwin, r.p.x, r.p.y, r.d.w, r.d.h, True);
                XFlush(display);
            }
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
