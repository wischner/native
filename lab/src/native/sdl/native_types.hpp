//
// native_types.hpp
// 
// Mapping standard nice types to SDL.
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 16.01.2021   tstih
// 
#ifndef _NATIVE_TYPES_HPP
#define _NATIVE_TYPES_HPP

#include "native_includes.hpp"

namespace nice {

//{{BEGIN.TYP}}
    // Unix process id.
    typedef pid_t app_id;

    // Basic X11 stuff.
    typedef int app_instance;

    // X11 coordinate.
    typedef int coord;

    // 8 bit integer.
    typedef uint8_t byte;

    // X11 GC and required stuff.
    typedef SDL_Renderer* canvas;
//{{END.TYP}}

}

#endif // _NATIVE_TYPES_HPP