#include <stdexcept>

#include <SDL2/SDL.h>

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
            sdl::wnd_bindings.unregister_by_b(this);
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
            SDL_Window *win = sdl::wnd_bindings.from_b(this);
            SDL_SetWindowPosition(win, p.x, p.y);
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
            SDL_Window *win = sdl::wnd_bindings.from_b(this);
            SDL_SetWindowSize(win, s.w, s.h);
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
            SDL_Window *win = sdl::wnd_bindings.from_b(this);
            SDL_SetWindowPosition(win, r.p.x, r.p.y);
            SDL_SetWindowSize(win, r.d.w, r.d.h);
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

        // SDL2 doesn't support native parent/child relationships
        // This would need to be implemented at the application level
        // For now, just store the parent reference

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

        // Trigger a repaint by posting an expose event
        SDL_Event event;
        event.type = SDL_WINDOWEVENT;
        event.window.event = SDL_WINDOWEVENT_EXPOSED;
        SDL_Window *win = sdl::wnd_bindings.from_b(const_cast<wnd *>(this));
        event.window.windowID = SDL_GetWindowID(win);
        SDL_PushEvent(&event);

        return const_cast<wnd &>(*this);
    }

    wnd &wnd::invalidate(const rect &r) const
    {
        // SDL2 doesn't support partial invalidation natively
        // Just invalidate the whole window
        return invalidate();
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
