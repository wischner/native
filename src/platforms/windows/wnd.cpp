#include <stdexcept>

#include <windows.h>
#include <windowsx.h>

#include <native.h>

#include "gpx_wnd.h"
#include "globals.h"

namespace win
{
    static native::mouse_button button_from_msg(UINT msg, WPARAM wParam)
    {
        switch (msg)
        {
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
            return native::mouse_button::left;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
            return native::mouse_button::right;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            return native::mouse_button::middle;
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
            return (HIWORD(wParam) == XBUTTON1)
                       ? native::mouse_button::x1
                       : native::mouse_button::x2;
        default:
            return native::mouse_button::none;
        }
    }

    LRESULT CALLBACK RoutedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (msg == WM_NCCREATE)
        {
            auto *create = reinterpret_cast<CREATESTRUCT *>(lParam);
            auto *native_wnd = reinterpret_cast<native::wnd *>(create->lpCreateParams);
            if (native_wnd)
            {
                SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(native_wnd));
                wnd_bindings.register_pair(hwnd, native_wnd);
            }
        }

        native::wnd *wnd = wnd_bindings.from_a(hwnd);
        if (!wnd)
        {
            wnd = reinterpret_cast<native::wnd *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
            if (wnd)
                wnd_bindings.register_pair(hwnd, wnd);
        }

        if (!wnd)
            return DefWindowProc(hwnd, msg, wParam, lParam);

        switch (msg)
        {
        case WM_MOVE:
            wnd->on_wnd_move.emit(native::point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
            break;

        case WM_SIZE:
            wnd->on_wnd_resize.emit(native::size(LOWORD(lParam), HIWORD(lParam)));
            break;

        case WM_MOUSEMOVE:
            wnd->on_mouse_move.emit(native::point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
            break;

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_XBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_XBUTTONUP:
        {
            const native::mouse_button btn = button_from_msg(msg, wParam);
            const native::mouse_action act = (msg == WM_LBUTTONDOWN || msg == WM_RBUTTONDOWN ||
                                              msg == WM_MBUTTONDOWN || msg == WM_XBUTTONDOWN)
                                                 ? native::mouse_action::press
                                                 : native::mouse_action::release;

            if (btn != native::mouse_button::none)
            {
                native::mouse_event me(
                    btn, act,
                    native::point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
                wnd->on_mouse_click.emit(me);
            }
            break;
        }

        case WM_MOUSEWHEEL:
#ifdef WM_MOUSEHWHEEL
        case WM_MOUSEHWHEEL:
#endif
        {
            POINT screen_pt{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            ScreenToClient(hwnd, &screen_pt);

            native::wheel_direction wdir = native::wheel_direction::vertical;
#ifdef WM_MOUSEHWHEEL
            if (msg == WM_MOUSEHWHEEL)
                wdir = native::wheel_direction::horizontal;
#endif
            native::mouse_wheel_event wheel(
                native::point(screen_pt.x, screen_pt.y),
                static_cast<native::coord>(GET_WHEEL_DELTA_WPARAM(wParam)),
                wdir);
            wnd->on_mouse_wheel.emit(wheel);
            break;
        }

        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);

            native::rect r(
                static_cast<native::coord>(ps.rcPaint.left),
                static_cast<native::coord>(ps.rcPaint.top),
                static_cast<native::dim>(ps.rcPaint.right - ps.rcPaint.left),
                static_cast<native::dim>(ps.rcPaint.bottom - ps.rcPaint.top));

            auto &g = wnd->get_gpx().set_clip(r);
            g.clear(native::rgba(255, 255, 255, 255));
            native::wnd_paint_event e{r, g};
            wnd->on_wnd_paint.emit(e);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_COMMAND:
            if (HIWORD(wParam) == 0)
            {
                // Menu item click
                if (auto *aw = dynamic_cast<native::app_wnd *>(wnd))
                    aw->on_menu.emit(static_cast<int>(LOWORD(wParam)));
            }
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;

        case WM_DESTROY:
            wnd_bindings.unregister_by_a(hwnd);
            if (wnd == native::app::main_wnd())
                PostQuitMessage(0);
            return 0;

        default:
            break;
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
} // namespace win

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
        if (auto *cache = win::wnd_gpx_bindings.from_a(this))
        {
            if (cache->pen)
                DeleteObject(cache->pen);
            if (cache->brush)
                DeleteObject(cache->brush);
            delete cache;
            win::wnd_gpx_bindings.unregister_by_a(this);
        }

        win::wnd_bindings.unregister_by_b(this);
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
            HWND hwnd = win::wnd_bindings.from_b(this);
            if (hwnd)
                SetWindowPos(hwnd, nullptr, p.x, p.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
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
            HWND hwnd = win::wnd_bindings.from_b(this);
            if (hwnd)
                SetWindowPos(hwnd, nullptr, 0, 0, s.w, s.h, SWP_NOMOVE | SWP_NOZORDER);
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
            HWND hwnd = win::wnd_bindings.from_b(this);
            if (hwnd)
                SetWindowPos(hwnd, nullptr, r.p.x, r.p.y, r.d.w, r.d.h, SWP_NOZORDER);
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
            HWND child = win::wnd_bindings.from_b(this);
            HWND parent = win::wnd_bindings.from_b(p);
            if (child && parent)
                SetParent(child, parent);
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

        HWND hwnd = win::wnd_bindings.from_b(const_cast<wnd *>(this));
        if (hwnd)
            InvalidateRect(hwnd, nullptr, FALSE);
        return const_cast<wnd &>(*this);
    }

    wnd &wnd::invalidate(const rect &r) const
    {
        if (!_created)
            return const_cast<wnd &>(*this);

        HWND hwnd = win::wnd_bindings.from_b(const_cast<wnd *>(this));
        if (hwnd)
        {
            RECT rect = {r.p.x, r.p.y, r.x2(), r.y2()};
            InvalidateRect(hwnd, &rect, FALSE);
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
