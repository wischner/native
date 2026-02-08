#include <native.h>
#include <bindings.h>
#include <windows.h>
#include <windowsx.h>
#include <iostream>

#include "gpx_wnd.h"
#include "globals.h"

namespace win {
    extern native::bindings<HWND, native::wnd*> wnd_bindings;
    extern native::bindings<native::wnd *, wingpx *> wnd_gpx_bindings;

    // Global custom WndProc
    LRESULT CALLBACK RoutedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        native::wnd* wnd = wnd_bindings.from_a(hwnd);

        if (wnd)
        {
            switch (msg)
            {
                case WM_CREATE:
                    wnd->on_wnd_create.emit();
                    break;

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
                case WM_LBUTTONUP:
                case WM_RBUTTONUP:
                case WM_MBUTTONUP:
                {
                    native::mouse_button button = native::mouse_button::none;
                    if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) button = native::mouse_button::left;
                    if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP) button = native::mouse_button::right;
                    if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONUP) button = native::mouse_button::middle;

                    native::mouse_event evt(button, native::point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)));
                    wnd->on_mouse_click.emit(evt);
                    break;
                }

                case WM_MOUSEWHEEL:
                {
                    native::mouse_wheel_event whe(
                        native::point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)),
                        GET_WHEEL_DELTA_WPARAM(wParam),
                        native::wheel_direction::vertical);
                    wnd->on_mouse_wheel.emit(whe);
                    break;
                }

                case WM_PAINT:
                {
                    PAINTSTRUCT ps;
                    HDC hdc = BeginPaint(hwnd, &ps);

                    // Convert RECT to native::rect
                    native::rect r(
                        ps.rcPaint.left,
                        ps.rcPaint.top,
                        ps.rcPaint.right - ps.rcPaint.left,
                        ps.rcPaint.bottom - ps.rcPaint.top);

                    // Get graphics context and set clip
                    auto &g = wnd->get_gpx().set_clip(r);

                    // Emit paint event
                    native::wnd_paint_event e{r, g};
                    wnd->on_wnd_paint.emit(e);

                    EndPaint(hwnd, &ps);
                    break;
                }
            }
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    // Free function to bind events
    void bind_events(HWND hwnd, native::wnd* wnd)
    {
        if (!hwnd || !wnd)
        {
            std::cerr << "Windows: Can't bind events — HWND or wnd is null.\n";
            return;
        }

        win::wnd_bindings.register_pair(hwnd, wnd);
        SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(win::RoutedWndProc));
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
        if (_created)
        {
            // Clean up graphics cache
            if (auto *cache = win::wnd_gpx_bindings.from_a(this))
            {
                if (cache->pen)
                    DeleteObject(cache->pen);
                if (cache->brush)
                    DeleteObject(cache->brush);
                if (cache->font)
                    DeleteObject(cache->font);
                delete cache;
                win::wnd_gpx_bindings.unregister_by_a(this);
            }

            win::wnd_bindings.unregister_by_b(this);
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
            HWND hwnd = win::wnd_bindings.from_b(this);
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
        InvalidateRect(hwnd, nullptr, TRUE);
        return const_cast<wnd &>(*this);
    }

    wnd &wnd::invalidate(const rect &r) const
    {
        if (!_created)
            return const_cast<wnd &>(*this);

        HWND hwnd = win::wnd_bindings.from_b(const_cast<wnd *>(this));
        RECT rect = {r.p.x, r.p.y, r.x2() + 1, r.y2() + 1};
        InvalidateRect(hwnd, &rect, TRUE);
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
