//
// native_wnd.hpp
// 
// Native window for SDL. 
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 02.06.2021   tstih
// 
#include "nice.hpp"

namespace nice {

//{{BEGIN.DEF}}
    // Static variable.
    std::map<SDL_Window*,native_wnd*> native_wnd::wmap_;

    native_wnd::native_wnd(wnd *window) {
        window_=window;
    }

    native_wnd::~native_wnd() {
        // And lazy destroy. 
        ::SDL_DestroyWindow(winst_);
    }

    void native_wnd::destroy() {
        // Remove me from windows map.
        wmap_.erase (winst_); 
    }

    void native_wnd::repaint() {
        // TODO: Whatever.
    }

    void native_wnd::set_title(std::string s) {
        ::SDL_SetWindowTitle(winst_, s.c_str());
    };
    
    std::string native_wnd::get_title() {
        return SDL_GetWindowTitle(winst_);
    }

    size native_wnd::get_wsize() {
        int w,h;
        ::SDL_GetWindowSize(winst_, &w, &h);
        return { w, h };
    }

    void native_wnd::set_wsize(size sz) {
        // TODO: Check SDL_GetRendererOutputSize
        ::SDL_SetWindowSize(winst_, sz.w, sz.h);
    }

    pt native_wnd::get_location() {
        int x,y;
        SDL_GetWindowPosition(winst_, &x, &y);
        return { x, y };
    }

    void native_wnd::set_location(pt location) {
        SDL_SetWindowPosition(winst_,location.x, location.y);
    }

    rct native_wnd::get_paint_area() {
        int w,h;
        ::SDL_GetWindowSize(winst_, &w, &h);
        return { 0,0,w,h };
    }

    // TODO:for now SDL only has one window so we're
    // assuming the first entry in the map, but we're
    // ready for more!
    bool native_wnd::global_wnd_proc(const SDL_Event& e) {
        if (wmap_.size()==0) return false;
        native_wnd* nw = wmap_.begin()->second;
        return nw->local_wnd_proc(e);
    }

    // Local (per window) window proc.
    bool native_wnd::local_wnd_proc(const SDL_Event& e) {
        bool quit=false;
        switch(e.type) {
            case SDL_WINDOWEVENT:
                switch (e.window.event) {
                    case SDL_WINDOWEVENT_EXPOSED:
                    {
                        // Paint event.
                        artist a(wrenderer_);
                        window_->paint.emit(a);
                        ::SDL_RenderPresent( wrenderer_ );
                    }
                    break;
                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                    {
                        // Resize event.
                        int width = e.window.data1;
                        int height = e.window.data2;
                        window_->resized.emit({width, height});
                    }
                    break;
                    case SDL_WINDOWEVENT_CLOSE:
                        // Destroy, I guess...
                        quit=true;
                        break;
                }
            break;
            case SDL_MOUSEBUTTONDOWN:
            break;
            case SDL_MOUSEBUTTONUP:
            break;
            case SDL_MOUSEMOTION:
            {
                mouse_info mi = {
                    {e.motion.x, e.motion.y}, // point
                    (bool)(e.motion.state & SDL_BUTTON_LMASK),
                    (bool)(e.motion.state & SDL_BUTTON_MMASK),
                    (bool)(e.motion.state & SDL_BUTTON_RMASK),
                    false, // TODO: Shift and Ctrl status.
                    false
                };
            }
            break;
        }
        return quit;
    }
//{{END.DEF}}

} // namespace nice