//
// native_wnd.cpp
// 
// Native window implementation for X11.
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 17.05.2021   tstih
// 
#include "nice.hpp"

namespace nice {

//{{BEGIN.DEF}}
    // Static variable.
    std::map<Window,native_wnd*> native_wnd::wmap_;

    native_wnd::native_wnd(wnd *window) {
        window_=window;
        display_=app::instance().display;
        // TODO: pass size.
        cached_wsize_={0,0}; // Used to recognize resize events.
    }

    native_wnd::~native_wnd() {
        // And lazy destroy. We could do this in the destroy() function.
        if (cached_gc_!=0) XFreeGC(display_,cached_gc_);
        XDestroyWindow(display_, winst_); winst_=0;
    }

    void native_wnd::destroy() {
        // Remove me from windows map.
        wmap_.erase (winst_); 
    }

    void native_wnd::repaint() {
        XClearArea(display_, winst_, 0, 0, 1, 1, true);
    }

    void native_wnd::set_title(std::string s) {
        ::XStoreName(display_, winst_, s.c_str());
    };
     
    std::string native_wnd::get_title() {
        char * name;
        ::XFetchName(display_,winst_, &name);
        std::string wname=name;
        ::XFree(name);
        return wname;
    }

    size native_wnd::get_wsize() {
        XWindowAttributes wattr;
        ::XGetWindowAttributes(display_,winst_,&wattr);
        // TODO: I think this is just the client area?
        return { wattr.width, wattr.height };
    }

    void native_wnd::set_wsize(size sz) {
        XResizeWindow(display_,winst_, sz.w, sz.h);
    }

    pt native_wnd::get_location() {
        XWindowAttributes wattr;
        ::XGetWindowAttributes(display_,winst_,&wattr);
        // TODO: I think this is just the client area?
        return { wattr.x, wattr.y };
    }

    void native_wnd::set_location(pt location) {
        XMoveWindow(display_,winst_, location.left, location.top);
    }

    // TODO: Implement.
    rct native_wnd::get_paint_area() {
        XWindowAttributes wattr;
        ::XGetWindowAttributes(display_,winst_,&wattr);
        return { 0, 0, wattr.width, wattr.height };
    }

    // Static (global) window proc. For all classes -
    // Remaps the call to local window proc.
    bool native_wnd::global_wnd_proc(const XEvent& e) {
        Window xw = e.xany.window;
        native_wnd* nw = wmap_[xw];
        return nw->local_wnd_proc(e);
    }

    // Local (per window) window proc.
    bool native_wnd::local_wnd_proc(const XEvent& e) {
        bool quit=false;
        switch ( e.type )
        {
        case CreateNotify:
            window_->created.emit();
            break;
        case ClientMessage:
            {
                Atom atom = XInternAtom ( display_,
                            "WM_DELETE_WINDOW",
                            false );
                if ( atom == e.xclient.data.l[0] )
                    window_->destroyed.emit();
                    quit=true;
            }
            break;
        case Expose:
            {
                // Need to create the gc?
                if (cached_gc_==0)
                    cached_gc_= XCreateGC(display_, winst_, 0, NULL); 
                canvas c { display_, winst_, cached_gc_};
                artist a(c);
                window_->paint.emit(a);
            }
		    break;
        case ButtonPress: // https://tronche.com/gui/x/xlib/events/keyboard-pointer/keyboard-pointer.html
        case ButtonRelease:
            {
            mouse_info mi = {
                { e.xmotion.x, e.xmotion.y },
                (bool)(e.xbutton.button&Button1), 
                (bool)(e.xbutton.button&Button2),
                (bool)(e.xbutton.button&Button3),
                (bool)(e.xbutton.state&ControlMask),
                (bool)(e.xbutton.state&ShiftMask)
            };
            if (e.type==ButtonPress)
                window_->mouse_down.emit(mi);
            else
                window_->mouse_up.emit(mi);
            }
            break;
        case MotionNotify:
            {
            mouse_info mi = {
                { e.xmotion.x, e.xmotion.y },
                (bool)(e.xmotion.state&Button1Mask), 
                (bool)(e.xmotion.state&Button2Mask),
                (bool)(e.xmotion.state&Button3Mask),
                (bool)(e.xmotion.state&ControlMask),
                (bool)(e.xmotion.state&ShiftMask)
            };
            window_->mouse_move.emit(mi);
            }
            break;
        case ConfigureNotify:
            {
            XConfigureEvent xce = e.xconfigure;
            // Size change?
            if (xce.width!=cached_wsize_.w || xce.height!=cached_wsize_.h) {
                cached_wsize_.w=xce.width;
                cached_wsize_.h=xce.height;
                window_->resized.emit({xce.width,xce.height});
            }
            }
            break;
        // TODO: KeyPress, KeyRelease
        } // switch
        return quit;
    }
//{{END.DEF}}

} // namespace nice
