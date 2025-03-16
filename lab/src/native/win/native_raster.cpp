//
// native_raster.cpp
// 
// Native raster (X11) format (for easy blitting).
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 10.06.2021   tstih
// 
#include "native_raster.hpp"

namespace nice {

//{{BEGIN.DEF}}
    native_raster::native_raster(int width, int height, const uint8_t *bgra) :
        native_raster(width,height) {
        // For windows we need 24 bit BGR array.
        uint8_t *dst=raw_.get();
        for (int i=0; i<width*height; i++) {
            int srci=4*i, dsti=3*i;
            dst[dsti] = bgra[srci];
            dst[dsti+1] = bgra[srci+1];
            dst[dsti+2] = bgra[srci+2];
        }
    }
    
    native_raster::native_raster(int width, int height) :
        width_(width), 
        height_(height) {

        // Calculate raster length.
        len_ = width * height * 3; // BGRA!
        // Allocate memory.
        raw_=std::make_unique<uint8_t[]>(len_);
    }  

    native_raster::~native_raster() {

    }

    int native_raster::width() const {
        return width_;
    }

    int native_raster::height() const {
        return height_;
    }

    uint8_t* native_raster::raw() const {
        return raw_.get();
    }
//{{END.DEF}}

}