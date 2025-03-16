//
// native_types.hpp
// 
// Mapping standard nice types to MS Windows types.
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 16.01.2021   tstih
// 
#ifndef _NATIVE_TYPES_HPP
#define _NATIVE_TYPES_HPP

#include "includes.hpp"

namespace nice {

//{{BEGIN.TYP}}
    // Mapped to Win32 process id.
    typedef DWORD  app_id;

    // Mapped to Win32 application instace (passed to WinMain)
    typedef HINSTANCE app_instance;

    // Screen coordinate for all geometry functions.
    typedef LONG coord;

    // 8 bit integer.
    typedef BYTE byte;

    // Mapped to device context.
    typedef HDC canvas;
//{{END.TYP}}

}

#endif // _NATIVE_TYPES_HPP