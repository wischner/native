//
// wnd.cpp
// 
// Window class implementation. 
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 09.05.2021   tstih
// 
#include "nice.hpp"

namespace nice {
//{{BEGIN.DEF}}
    void wnd::repaint(void) { native()->repaint(); }
    
    std::string wnd::get_title() { return native()->get_title(); }
    
    void wnd::set_title(std::string s) { native()->set_title(s); } 
    
    size wnd::get_wsize() { return native()->get_wsize(); }
    
    void wnd::set_wsize(size sz) { native()->set_wsize(sz); } 
    
    pt wnd::get_location() { return native()->get_location(); }
    
    void wnd::set_location(pt location) { native()->set_location(location); } 

    rct wnd::get_paint_area() { return native()->get_paint_area(); };
//{{END.DEF}}
}