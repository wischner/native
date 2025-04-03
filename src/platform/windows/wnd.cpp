#include <native.h>
#include <bindings.h>
#include <windows.h>
#include <windowsx.h>
#include <iostream>

namespace win {
    native::bindings<HWND, native::wnd*> wnd_bindings;

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
