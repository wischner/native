//
// native_app_wnd.cpp
// 
// Native application window implementation for SDL.
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 02.06.2021   tstih
// 
#include "nice.hpp"

namespace nice {

//{{BEGIN.DEF}}
    native_app_wnd::native_app_wnd(
        app_wnd *window,
        std::string title,
        size size
    ) : native_wnd(window) {
        /* Create window. In screen coordinates. */
        winst_ = ::SDL_CreateWindow(title.c_str(), 
            SDL_WINDOWPOS_UNDEFINED, 
            SDL_WINDOWPOS_UNDEFINED, 
            size.w, 
            size.h, 
            SDL_WINDOW_HIDDEN);

        // Store window to window list.
        wmap_.insert(std::pair<SDL_Window*,native_wnd*>(winst_, this));

        // Get window surface.
        wrenderer_=::SDL_CreateRenderer( winst_, -1, SDL_RENDERER_ACCELERATED);
    }

    native_app_wnd::~native_app_wnd() {
        ::SDL_DestroyRenderer(wrenderer_);
    }

    void native_app_wnd::show() const { 
        ::SDL_ShowWindow(winst_);
    }
//{{END.DEF}}

} // namespace nice