//
// native_wnd.hpp
// 
// Native window for X11. 
// Three "window" structures are involved here.
//  1. X11 Window structure is native to Xlib.
//  2. native_wnd is C++ adapter for X11 window
//  3. wnd is "clean" (non-native!) window structure of the nice library.
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 17.05.2021   tstih
// 
#ifndef _NATIVE_WND_H
#define _NATIVE_WND_H

namespace nice {

//{{BEGIN.DEC}}
    class wnd; // Forward declaration.
    class native_wnd {
    public:
        // Ctor creates X11 window. 
        native_wnd(wnd *window);
        // Dtor.
        virtual ~native_wnd();
        // Destroy native window.
        void destroy(void);
        // Invalidate native window.
        void repaint(void);
        // Get window title.
        std::string get_title();
        // Set window title.
        void set_title(std::string s);
        // Get window size (not client size). 
        size get_wsize();
        // Set window size.
        void set_wsize(size sz);
        // Get window relative location (to parent)
        // or absolute location if parent is screen. 
        pt get_location();
        // Set window location.
        void set_location(pt location);
        // Get window paint rectangle.
        rct get_paint_area();
        // Global window procedure (static)
        static bool global_wnd_proc(const XEvent& e);
    protected:
        // X11 window structure.
        Window winst_; 
        // X11 display.
        Display* display_;
        // A map from X11 window to native_wnd.
        static std::map<Window,native_wnd*> wmap_;
        // Local window procdure.
        virtual bool local_wnd_proc(const XEvent& e);
        // Pointer to related non-native window struct.
        wnd* window_;
        // Cached size.
        size cached_wsize_;
        GC cached_gc_ {0};
    };
//{{END.DEC}}

} // namespace nice

#endif // _NATIVE_WND_H 