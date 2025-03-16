//
// native_wnd.hpp
// 
// Native window for SDL. 
// Three "window" structures are involved here.
//  1. SDL native window structure.
//  2. native_wnd is C++ adapter for window
//  3. wnd is "clean" (non-native!) window structure of the nice library.
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 02.06.2021   tstih
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
        static bool global_wnd_proc(const SDL_Event& e);
    protected:
        // Native SDL window structure.
        SDL_Window* winst_; 
        // Window surface.
        SDL_Renderer *wrenderer_;
        // A map from X11 window to native_wnd.
        static std::map<SDL_Window *,native_wnd*> wmap_;
        // Local window procdure.
        virtual bool local_wnd_proc(const SDL_Event& e);
        // Pointer to related non-native window struct.
        wnd* window_;
    };
//{{END.DEC}}

} // namespace nice

#endif // _NATIVE_WND_H 