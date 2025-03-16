//
// native_wnd.cpp
// 
// Native window implementation for MS Windows.
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 09.05.2021   tstih
// 
#include "nice.hpp"

namespace nice {

//{{BEGIN.DEF}}
    void native_wnd::destroy(void) {
        ::PostQuitMessage(0);
    }

    native_wnd::native_wnd(wnd *window) {
        window_=window;
    }

    native_wnd::~native_wnd() {
        ::DestroyWindow(hwnd_);
    }

    void native_wnd::repaint(void) {
         ::InvalidateRect(hwnd_, NULL, TRUE);
    }

    std::string native_wnd::get_title() {
        TCHAR szTitle[1024];
        ::GetWindowTextA(hwnd_, szTitle, 1024);
        return std::string(szTitle);
    }

    void native_wnd::set_title(std::string s) {
        ::SetWindowText(hwnd_,s.c_str());
    }

    size native_wnd::get_wsize() {
        RECT wr;
        ::GetWindowRect(hwnd_, &wr);
        return size{ wr.right-wr.left+1, wr.bottom-wr.top+1 };
    }

    void native_wnd::set_wsize(size sz) {
         // Use move.
        RECT wr;
        ::GetWindowRect(hwnd_, &wr);
        ::MoveWindow(hwnd_, wr.left, wr.top, sz.w, sz.h, TRUE);
    }

    pt native_wnd::get_location() {
        RECT wr;
        ::GetWindowRect(hwnd_, &wr);
        return pt{ wr.left, wr.top };
    }

    void native_wnd::set_location(pt location) {
        // We need to keep the position and just change the size.
        RECT wr;
        ::GetWindowRect(hwnd_, &wr);
        ::MoveWindow(hwnd_, 
            location.left, 
            location.top, 
            wr.right-wr.left+1, 
            wr.bottom-wr.top+1, TRUE);
    }

    rct native_wnd::get_paint_area() {
        RECT client;
        ::GetClientRect(hwnd_, &client);
        return { client.left, client.top, client.right, client.bottom };
    }

    LRESULT CALLBACK native_wnd::global_wnd_proc(
        HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
        
        // Is it the very first message? Only on WM_NCCREATE.
        // TODO: Why does Windows 10 send WM_GETMINMAXINFO first?!
        native_wnd* self = nullptr;
        if (message == WM_NCCREATE) {
            LPCREATESTRUCT lpcs = reinterpret_cast<LPCREATESTRUCT>(lParam);
            auto self = static_cast<native_wnd*>(lpcs->lpCreateParams);
            self->hwnd_=hWnd; // save the window handle too!
            ::SetWindowLongPtr(
                hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        else
            self = reinterpret_cast<native_wnd*>
                (::GetWindowLongPtr(hWnd, GWLP_USERDATA));

        // Chain...
        if (self != nullptr)
            return (self->local_wnd_proc(message, wParam, lParam));
        else
            return ::DefWindowProc(hWnd, message, wParam, lParam);
    }
    
    LRESULT native_wnd::local_wnd_proc(
        UINT msg, WPARAM wparam, LPARAM lparam) {

        switch (msg) {
            case WM_CREATE:
                window_->created.emit();
                break;
            case WM_DESTROY:
                window_->destroyed.emit();
                break;
            case WM_PAINT:
            {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd_, &ps);
                artist a(hdc);
                window_->paint.emit(a);
                EndPaint(hwnd_, &ps);
            }
            break;
            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_RBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_MBUTTONUP:
            case WM_RBUTTONUP:
            {
                // Populate the mouse info structure.
                mouse_info mi = {
                    {GET_X_LPARAM(lparam),GET_Y_LPARAM(lparam)}, // point
                    (bool)(wparam & MK_LBUTTON),
                    (bool)(wparam & MK_MBUTTON),
                    (bool)(wparam & MK_RBUTTON),
                    (bool)(wparam & MK_CONTROL),
                    (bool)(wparam & MK_SHIFT)
                };
                if (msg == WM_MOUSEMOVE)
                    window_->mouse_move.emit(mi);
                else if (msg == WM_LBUTTONDOWN || msg == WM_MBUTTONDOWN || msg == WM_RBUTTONDOWN)
                    window_->mouse_down.emit(mi);
                else
                    window_->mouse_up.emit(mi);
            }
            break;
            case WM_SIZE:
            {
                rct r = rct{ 0, 0, LOWORD(lparam), HIWORD(lparam) };
                window_->resized.emit(
                    {
                        LOWORD(lparam),
                        HIWORD(lparam)
                    }
                );
            }
                break;
            default:
                return ::DefWindowProc(hwnd_, msg, wparam, lparam);
            }
            return 0;

    }
//{{END.DEF}}

} // namespace nice