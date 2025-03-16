//
// artist.hpp
// 
// Class encapsulating all drawing in nice.
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 05.05.2021   tstih
// 
#ifndef _ARTIST_HPP
#define _ARTIST_HPP

#include "raster.hpp"

namespace nice {

//{{BEGIN.DEC}}
    class artist {
    public:
        // Pass canvas instance, don't own it.
        artist(const canvas& canvas) {
            canvas_ = canvas;
        }
        // Methods.
        void draw_line(color c, pt p1, pt p2) const;
        void draw_rect(color c, rct r) const;
        void fill_rect(color c, rct r) const;
        void draw_raster(const raster& rst, pt p) const;
    private:
        // Passed canvas.
        canvas canvas_;
    };
//{{END.DEC}}

}

#endif