//
// property.hpp
// 
// Property variables for nice.
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 16.01.2021   tstih
// 
#ifndef _PROPERTY_HPP
#define _PROPERTY_HPP

#include "includes.hpp"

namespace nice {

//{{BEGIN.DEC}}
    template<typename T>
    class ro_property {
    public:
        ro_property(
            std::function<T()> getter) :
            getter_(getter) { }
        operator T() const { return getter_(); }
    private:
        std::function<T()> getter_;
    };

    template<typename T>
    class property {
    public:
        property(
            std::function<void(T)> setter,
            std::function<T()> getter) :
            setter_(setter), getter_(getter) { }
        operator T() const { return getter_(); }
        property<T>& operator= (const T& value) { setter_(value); return *this; }
    private:
        std::function<void(T)> setter_;
        std::function<T()> getter_;
    };
//{{END.DEC}}

}

#endif // _PROPERTY_HPP