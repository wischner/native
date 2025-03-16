//
// raster.hpp
// 
// Raster image (32bpp raw data). This is used for all 
// images in nice.
//
// TODO:
//  Only one constructor with default parameter
//  Switch to actual argb
//
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 02.06.2021   tstih
// 
#ifndef _RASTER_H
#define _RASTER_H

#include <cstdint>
#include <memory>

namespace nice {
//{{BEGIN.DEC}}
    class raster {
    public:
        // Constructs a new raster.        
        raster(int width, int height) {
            native_=std::make_unique<native_raster>(width, height);
        }
        // Construct a raster from resource.
        raster(int width, int height, const uint8_t * argb) {
            native_=std::make_unique<native_raster>(width, height, argb);
        }
        // Destructs the raster.
        virtual ~raster() {};
        // Width.
        int width() const { return native_->width(); }
        // Height.
        int height() const { return native_->height(); }
        // Pointer to raw data.
        uint8_t* raw() const { return native_->raw(); }
    private:
        // PIMPL.
        std::unique_ptr<native_raster> native_;
    };
//{{END.DEC}}
} // namespace nice

#endif