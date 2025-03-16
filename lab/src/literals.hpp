//
// literals.hpp
// 
// User defined literals for nice: percent, pixel.
//
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 16.01.2021   tstih
// 
#ifndef _LITERALS_HPP
#define _LITERALS_HPP

namespace nice {

//{{BEGIN.DEC}}
   class percent
    {
        double percent_;
    public:
        class pc {};
        explicit constexpr percent(pc, double dpc) : percent_{ dpc } {}
    };

    class pixel
    {
        int pixel_;
    public:
        class px {};
        explicit constexpr pixel(px, int ipx) : pixel_{ ipx } {}
        int value() { return pixel_; }
    };
//{{END.DEC}}

//{{BEGIN.DEF}}
    constexpr percent operator "" _pc(long double dpc)
    {
        return percent{ percent::pc{}, static_cast<double>(dpc) };
    }

    constexpr pixel operator "" _px(unsigned long long ipx)
    {
        return pixel{ pixel::px{}, static_cast<int>(ipx) };
    }
//{{END.DEF}}

}

#endif // _LITERALS_HPP