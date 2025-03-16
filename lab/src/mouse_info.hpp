//
// mouse_info.hpp
// 
// The structure for mouse signals (i.e. mouse move, mouse down, mouse up, etc.).
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 16.01.2021   tstih
// 
#ifndef _MOUSE_INFO_HPP
#define _MOUSE_INFO_HPP

namespace nice {

//{{BEGIN.DEC}}
    // Buton status: true=down, false=up.
    struct mouse_info {
        pt location;
        bool left_button;       
        bool middle_button;
        bool right_button;
        bool ctrl;
        bool shift;
    };
//{{END.DEC}}

}

#endif // _MOUSE_INFO_HPP