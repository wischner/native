#include <native.h>
#include <bindings.h>
#include <windows.h>
#include <iostream>

namespace win {
    extern native::bindings<HWND, native::wnd*> wnd_bindings;

    // Custom window procedure
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
                {
                    int x = (int)(short)LOWORD(lParam);
                    int y = (int)(short)HIWORD(lParam);
                    wnd->on_wnd_move.emit(native::point(x, y));
                    break;
                }

                case WM_SIZE:
                {
                    int w = LOWORD(lParam);
                    int h = HIWORD(lParam);
                    wnd->on_wnd_resize.emit(native::size(w, h));
                    break;
                }

                case WM_MOUSEMOVE:
                {
                    int x = GET_X_LPARAM(lParam);
                    int y = GET_Y_LPARAM(lParam);
                    wnd->on_mouse_move.emit(native::point(x, y));
                    break;
                }

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

                    int x = GET_X_LPARAM(lParam);
                    int y = GET_Y_LPARAM(lParam);

                    native::mouse_event me(button, native::point(x, y));
                    wnd->on_mouse_click.emit(me);
                    break;
                }

                case WM_MOUSEWHEEL:
                {
                    int delta = GET_WHEEL_DELTA_WPARAM(wParam);
                    native::mouse_wheel_event whe(
                        native::point(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)),
                        delta,
                        native::wheel_direction::vertical);
                    wnd->on_mouse_wheel.emit(whe);
                    break;
                }

                // TODO: Handle keyboard or paint if needed
            }
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
}

namespace native {

void wnd::bind_events() const
{
    HWND hwnd = win::wnd_bindings.from_b(this);
    if (!hwnd)
    {
        std::cerr << "Windows: Can't bind events — HWND not found.\n";
        return;
    }

    // Subclass the window procedure
    SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(win::RoutedWndProc));
}

} // namespace native
