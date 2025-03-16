//
// geometry.hpp
// 
// Nice geometry clases (rectangle, point, etc.) 
// TODO: Include types.h. 
//
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 16.01.2021   tstih
// 
#ifndef _GEOMETRY_HPP
#define _GEOMETRY_HPP

namespace nice {

//{{BEGIN.DEC}}
    typedef struct size_s {
        union { coord width; coord w; };
        union { coord height; coord h; };
    } size;

    typedef struct color_s {
        byte r;
        byte g;
        byte b;
        byte a;
    } color;

    typedef struct pt_s {
        union { coord left; coord x; };
        union { coord top; coord y; };
    } pt;

    typedef struct rct_s {
        union { coord left; coord x; coord x1; };
        union { coord top; coord y;  coord y1; };
        union { coord width; coord w; };
        union { coord height; coord h; };
        coord x2() { return left + width; }
        coord y2() { return top + height; }
    } rct;
//{{END.DEC}}

}

#endif // _GEOMETRY_HPP