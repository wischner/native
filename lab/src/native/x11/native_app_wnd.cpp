//
// native_app_wnd.cpp
// 
// Native application window implementation for X11.
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

        int s = DefaultScreen(display_);
        winst_ = ::XCreateSimpleWindow(
            display_, 
            RootWindow(display_, s), 
            10, // x 
            10, // y
            size.width, 
            size.height, 
            1, // border width
            BlackPixel(display_, s), // border color
            WhitePixel(display_, s)  // background color
        );
        // Store window to window list.
        wmap_.insert(std::pair<Window,native_wnd*>(winst_, this));
        // Set initial title.
        ::XSetStandardProperties(display_,winst_,title.c_str(),NULL,None,NULL,0,NULL);

        // Rather strange handling of close window by X11.
        Atom atom = XInternAtom ( display_,"WM_DELETE_WINDOW", false );
        ::XSetWMProtocols(display_, winst_, &atom, 1);

        // TODO: Implement lazy subscription (somday)
        ::XSelectInput (display_, winst_,
			ExposureMask | ButtonPressMask | ButtonReleaseMask | EnterWindowMask | 
            LeaveWindowMask | PointerMotionMask | FocusChangeMask | KeyPressMask |
            KeyReleaseMask | SubstructureNotifyMask | StructureNotifyMask | 
            SubstructureRedirectMask);
    }

    native_app_wnd::~native_app_wnd() {}

    void native_app_wnd::show() const { 
        ::XMapWindow(display_, winst_);
    }
//{{END.DEF}}

} // namespace nice