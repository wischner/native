//
// native_types.hpp
// 
// Mapping standard nice types to MS Windows types.
// 
// (c) 2020 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 16.01.2020   tstih
// 
#ifndef _TYPES_HPP
#define _TYPES_HPP

#include "includes.hpp"

namespace nice {

//{{BEGIN.TYP}}
    // Unix process id.
    typedef pid_t app_id;

    // Basic X11 stuff.
    typedef struct x11_app_instance {
        Display* display;
    } app_instance;

    // X11 coordinate.
    typedef int coord;

    // 8 bit integer.
    typedef uint8_t byte;

    // X11 GC and required stuff.
    typedef struct x11_canvas {
        Display* d;
        Window w;
        GC gc;
    } canvas;
//{{END.TYP}}

}

#endif // _TYPES_HPP