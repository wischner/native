//
// native_raster.hpp
// 
// Native raster (x11) header.
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 10.06.2021   tstih
// 
#ifndef _NATIVE_RASTER_HPP
#define _NATIVE_RASTER_HPP

#include <cstdint>
#include <memory>

namespace nice {

//{{BEGIN.DEC}}
    class native_raster {
    public:
        // Construct a raster from resource.
        native_raster(int width, int height, const uint8_t *bgra);
        // Allocate resource.
        native_raster(int width, int height);  
        virtual ~native_raster();
        // Width.
        int width() const;
        // Height.
        int height() const;
        // Pointer to raw data.
        uint8_t* raw() const;
    private:
        int width_, height_, len_;
        std::unique_ptr<uint8_t[]> raw_; // We own this!
    };
//{{END.DEC}}

} // namespace nice

#endif // _NATIVE_RASTER_HPP