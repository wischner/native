//
// Implements the Windows window backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>

#include <windows.h>
#include <windowsx.h>

#include <native.h>
#include <native/wnd.h>

#include "gpx_wnd.h"
#include "globals.h"

namespace windows
{
    static void app_window_frame_size(HWND hwnd,
                                      LONG &width,
                                      LONG &height) {
        RECT client = {0, 0, width, height};
        const DWORD style = static_cast<DWORD>(
            GetWindowLongPtrW(hwnd, GWL_STYLE));
        const DWORD extended_style = static_cast<DWORD>(
            GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
        if (AdjustWindowRectEx(
                &client,
                style,
                GetMenu(hwnd) != nullptr,
                extended_style)) {
            width = client.right - client.left;
            height = client.bottom - client.top;
        }
    }

    static bool is_user_input_message(UINT message) {
        return (message >= WM_KEYFIRST && message <= WM_KEYLAST) ||
               (message >= WM_MOUSEFIRST &&
                message <= WM_MOUSELAST) ||
               message == WM_COMMAND || message == WM_SYSCOMMAND;
    }

    static native::mouse_button button_from_msg(UINT message,
                                                WPARAM wparam) {
        switch (message) {
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
            return (HIWORD(wparam) == XBUTTON1)
                       ? native::mouse_button::x1
                       : native::mouse_button::x2;
        default:
            return native::mouse_button::none;
        }
    }

    LRESULT CALLBACK routed_wnd_proc(HWND hwnd,
                                     UINT message,
                                     WPARAM wparam,
                                     LPARAM lparam) {
        if (message == WM_NCCREATE) {
            auto *create = reinterpret_cast<CREATESTRUCT *>(lparam);
            auto *native_wnd =
                reinterpret_cast<native::wnd *>(create->lpCreateParams);
            if (native_wnd) {
                SetWindowLongPtr(
                    hwnd,
                    GWLP_USERDATA,
                    reinterpret_cast<LONG_PTR>(native_wnd));
                wnd_bindings.register_pair(hwnd, native_wnd);
            }
        }

        native::wnd *wnd = wnd_bindings.object_from_handle(hwnd);
        if (!wnd) {
            wnd = reinterpret_cast<native::wnd *>(
                GetWindowLongPtr(hwnd, GWLP_USERDATA));
            if (wnd)
                wnd_bindings.register_pair(hwnd, wnd);
        }

        if (!wnd)
            return DefWindowProcW(hwnd, message, wparam, lparam);

        if (!wnd->get_input_enabled() &&
            is_user_input_message(message)) {
            return 0;
        }

        switch (message) {
        case WM_MOVE: {
            native::point position(GET_X_LPARAM(lparam),
                                   GET_Y_LPARAM(lparam));
            if (dynamic_cast<native::app_wnd *>(wnd)) {
                RECT bounds = {};
                if (GetWindowRect(hwnd, &bounds)) {
                    position = native::point(
                        static_cast<native::coord>(bounds.left),
                        static_cast<native::coord>(bounds.top));
                }
            }
            wnd->on_native_move(position);
            wnd->on_wnd_move.emit(position);
            break;
        }

        case WM_SIZE: {
            native::size s(LOWORD(lparam), HIWORD(lparam));
            wnd->on_native_resize(s);
            wnd->on_wnd_resize.emit(s);
            break;
        }

        case WM_MOUSEMOVE:
            wnd->on_mouse_move.emit(native::point(
                GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)));
            break;

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_XBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_XBUTTONUP: {
            const native::mouse_button btn =
                button_from_msg(message, wparam);
            const bool is_press = message == WM_LBUTTONDOWN ||
                                  message == WM_RBUTTONDOWN ||
                                  message == WM_MBUTTONDOWN ||
                                  message == WM_XBUTTONDOWN;
            const native::mouse_action act =
                is_press ? native::mouse_action::press
                         : native::mouse_action::release;

            if (btn != native::mouse_button::none) {
                native::mouse_event me(
                    btn,
                    act,
                    native::point(GET_X_LPARAM(lparam),
                                  GET_Y_LPARAM(lparam)));
                wnd->on_mouse_click.emit(me);
            }
            break;
        }

        case WM_MOUSEWHEEL:
#ifdef WM_MOUSEHWHEEL
        case WM_MOUSEHWHEEL:
#endif
        {
            POINT screen_pt{GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam)};
            ScreenToClient(hwnd, &screen_pt);

            native::wheel_direction wdir =
                native::wheel_direction::vertical;
#ifdef WM_MOUSEHWHEEL
            if (message == WM_MOUSEHWHEEL)
                wdir = native::wheel_direction::horizontal;
#endif
            native::mouse_wheel_event wheel(
                native::point(screen_pt.x, screen_pt.y),
                static_cast<native::coord>(
                    GET_WHEEL_DELTA_WPARAM(wparam)),
                wdir);
            wnd->on_mouse_wheel.emit(wheel);
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);

            native::rect r(static_cast<native::coord>(ps.rcPaint.left),
                           static_cast<native::coord>(ps.rcPaint.top),
                           static_cast<native::dim>(ps.rcPaint.right -
                                                    ps.rcPaint.left),
                           static_cast<native::dim>(ps.rcPaint.bottom -
                                                    ps.rcPaint.top));

            auto &g = wnd->get_gpx().set_clip(r);
            g.clear(native::rgba(255, 255, 255, 255));
            native::wnd_paint_event e{r, g};
            wnd->on_wnd_paint.emit(e);

            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_COMMAND:
            if (lparam != 0) {
                HWND control = reinterpret_cast<HWND>(lparam);
                if (auto *child =
                        windows::wnd_bindings.object_from_handle(
                            control)) {
                    if (auto *btn =
                            dynamic_cast<native::button *>(child)) {
                        btn->on_click.emit();
                        return 0;
                    }
                    if (auto *check =
                            dynamic_cast<native::check *>(child)) {
                        if (HIWORD(wparam) == BN_CLICKED) {
                            check->on_native_checked(
                                SendMessageW(
                                    control, BM_GETCHECK, 0, 0) ==
                                BST_CHECKED);
                        }
                        return 0;
                    }
                    if (auto *radio =
                            dynamic_cast<native::radio *>(child)) {
                        if (HIWORD(wparam) == BN_CLICKED)
                            radio->on_native_selected();
                        return 0;
                    }
                    if (auto *list =
                            dynamic_cast<native::list *>(child)) {
                        if (HIWORD(wparam) == LBN_SELCHANGE) {
                            list->on_native_selection(
                                static_cast<int>(SendMessageW(
                                    control, LB_GETCURSEL, 0, 0)));
                        }
                        return 0;
                    }
                    if (auto *editor =
                            dynamic_cast<native::text_edit *>(child)) {
                        if (HIWORD(wparam) == EN_CHANGE)
                            windows::handle_text_edit_change(editor);
                        return 0;
                    }
                }
            } else if (HIWORD(wparam) == 0) {
                // Menu item click (lparam == 0).
                if (auto *aw = dynamic_cast<native::app_wnd *>(wnd)) {
                    aw->on_menu.emit(static_cast<int>(LOWORD(wparam)));
                    return 0;
                }
            }
            return 0;

        case WM_ERASEBKGND:
            return 1;

        case WM_CLOSE:
            wnd->destroy();
            return 0;

        case WM_DESTROY:
            wnd->on_native_destroy();
            wnd_bindings.unregister_by_handle(hwnd);
            if (wnd == native::app::main_wnd())
                PostQuitMessage(0);
            return 0;

        default:
            break;
        }

        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
} // namespace windows

namespace native
{
    void wnd::apply_position() {
        HWND hwnd = windows::wnd_bindings.handle_from_object(this);
        if (hwnd) {
            SetWindowPos(hwnd,
                         nullptr,
                         _bounds.p.x,
                         _bounds.p.y,
                         0,
                         0,
                         SWP_NOSIZE | SWP_NOZORDER);
        }
    }

    void wnd::apply_dimensions() {
        HWND hwnd = windows::wnd_bindings.handle_from_object(this);
        if (hwnd) {
            LONG width = _bounds.d.w;
            LONG height = _bounds.d.h;
            if (dynamic_cast<app_wnd *>(this))
                windows::app_window_frame_size(hwnd, width, height);
            SetWindowPos(hwnd,
                         nullptr,
                         0,
                         0,
                         width,
                         height,
                         SWP_NOMOVE | SWP_NOZORDER);
        }
    }

    void wnd::apply_bounds() {
        HWND hwnd = windows::wnd_bindings.handle_from_object(this);
        if (hwnd) {
            LONG width = _bounds.d.w;
            LONG height = _bounds.d.h;
            if (dynamic_cast<app_wnd *>(this))
                windows::app_window_frame_size(hwnd, width, height);
            SetWindowPos(hwnd,
                         nullptr,
                         _bounds.p.x,
                         _bounds.p.y,
                         width,
                         height,
                         SWP_NOZORDER);
        }
    }

    void wnd::apply_parent() {
        HWND child = windows::wnd_bindings.handle_from_object(this);
        HWND parent =
            _parent ? windows::wnd_bindings.handle_from_object(_parent)
                    : nullptr;
        if (child)
            SetParent(child, parent);
    }

    wnd &wnd::invalidate() const {
        if (!_created)
            return const_cast<wnd &>(*this);

        HWND hwnd = windows::wnd_bindings.handle_from_object(
            const_cast<wnd *>(this));
        if (hwnd)
            InvalidateRect(hwnd, nullptr, FALSE);
        return const_cast<wnd &>(*this);
    }

    wnd &wnd::invalidate(const rect &r) const {
        if (!_created)
            return const_cast<wnd &>(*this);

        HWND hwnd = windows::wnd_bindings.handle_from_object(
            const_cast<wnd *>(this));
        if (hwnd) {
            RECT rect = {r.p.x, r.p.y, r.x2(), r.y2()};
            InvalidateRect(hwnd, &rect, FALSE);
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
