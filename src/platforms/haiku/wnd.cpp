#include <stdexcept>

#include <Window.h>
#include <View.h>

#include <native.h>

#include "gpx_wnd.h"
#include "globals.h"

namespace
{
    template <typename Fn>
    void with_locked_window(BWindow *window, Fn &&fn)
    {
        if (!window)
            return;

        const bool already_locked = window->IsLocked();
        if (!already_locked && !window->Lock())
            return;

        fn(window);

        if (!already_locked)
            window->Unlock();
    }
} // namespace

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
            // Clean up graphics cache
            if (auto *cache = haiku::wnd_gpx_bindings.from_a(this))
            {
                // Note: BView is owned by BWindow, don't delete it
                delete cache;
                haiku::wnd_gpx_bindings.unregister_by_a(this);
            }

            haiku::wnd_bindings.unregister_by_b(this);
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
            BWindow *bwin = haiku::wnd_bindings.from_b(this);
            with_locked_window(bwin, [&](BWindow *window) { window->MoveTo(p.x, p.y); });
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
            BWindow *bwin = haiku::wnd_bindings.from_b(this);
            with_locked_window(bwin, [&](BWindow *window) { window->ResizeTo(s.w, s.h); });
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
            BWindow *bwin = haiku::wnd_bindings.from_b(this);
            with_locked_window(bwin, [&](BWindow *window) {
                window->MoveTo(r.p.x, r.p.y);
                window->ResizeTo(r.d.w, r.d.h);
            });
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

        // Haiku doesn't support reparenting windows after creation
        // Parent relationship is set during window creation

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

        BWindow *bwin = haiku::wnd_bindings.from_b(const_cast<wnd *>(this));
        with_locked_window(bwin, [](BWindow *window) {
            BView *view = window->ChildAt(0);
            if (view)
                view->Invalidate();
        });

        return const_cast<wnd &>(*this);
    }

    wnd &wnd::invalidate(const rect &r) const
    {
        if (!_created)
            return const_cast<wnd &>(*this);

        BWindow *bwin = haiku::wnd_bindings.from_b(const_cast<wnd *>(this));
        with_locked_window(bwin, [&](BWindow *window) {
            BView *view = window->ChildAt(0);
            if (!view)
                return;

            BRect rect(r.p.x, r.p.y, r.x2(), r.y2());
            view->Invalidate(rect);
        });

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
