//
// resize_info.hpp
// 
// The structure for windows resize signal.
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 05.05.2021   tstih
// 
#ifndef _RESIZEE_INFO_HPP
#define _RESIZE_INFO_HPP

namespace nice {

//{{BEGIN.DEC}}
    struct resized_info {
        coord width;
        coord height;
    };
//{{END.DEC}}

}

#endif // _RESIZE_INFO_HPP