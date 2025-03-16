//
// native_app_wnd.cpp
// 
// Native application window implementation for MS Windows.
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 09.05.2021   tstih
// 
#include "nice.hpp"

namespace nice {

//{{BEGIN.DEF}}
    native_app_wnd::native_app_wnd(
        app_wnd *window,
        std::string title,
        size size
    ) : native_wnd(window) {

        // Create app window.
        class_ = app::name();

        // Register window.
        ::ZeroMemory(&wcex_, sizeof(WNDCLASSEX));
        wcex_.cbSize = sizeof(WNDCLASSEX);
        wcex_.lpfnWndProc = global_wnd_proc;
        wcex_.hInstance = app::instance();
        wcex_.lpszClassName = class_.c_str();
        wcex_.hCursor = ::LoadCursor(NULL, IDC_ARROW);

        if (!::RegisterClassEx(&wcex_)) 
            throw_ex(nice_exception,"Unable to register class.");

        // Create it.
        hwnd_ = ::CreateWindowEx(
            0,
            class_.c_str(),
            title.c_str(),
            WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT,
            size.width, size.height,
            NULL,
            NULL,
            app::instance(),
            this);

        if (!hwnd_)
            throw_ex(nice_exception,"Unable to create window.");
    }

    native_app_wnd::~native_app_wnd() {}

    void native_app_wnd::show() const { 
        ::ShowWindow(hwnd_, SW_SHOWNORMAL); 
    }
//{{END.DEF}}

} // namespace nice
