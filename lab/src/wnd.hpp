//
// wnd.hpp
// 
// Window class. 
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 17.01.2021   tstih
// 
#ifndef _WND_HPP
#define _WND_HPP

#include "includes.hpp"
#include "resource.hpp"
#include "property.hpp"
#include "signal.hpp"
#include "geometry.hpp"

namespace nice {

//{{BEGIN.DEC}}
    class wnd  {
    public:
        // Methods.
        void repaint(void);

        // Properties.
        property<std::string> title {
            [this](std::string s) { this->set_title(s); },
            [this]() -> std::string {  return this->get_title(); }
        };

        property<size> wsize {
            [this](size sz) { this->set_wsize(sz); },
            [this]() -> size {  return this->get_wsize(); }
        };

        property<pt> location {
            [this](pt p) { this->set_location(p); },
            [this]() -> pt {  return this->get_location(); }
        };

        ro_property<rct> paint_area{
            [this]() -> rct { return this->get_paint_area(); }
        };

        // Signals.
        signal<> created;
        signal<> destroyed;
        signal<const artist&> paint;
        signal<const resized_info&> resized;
        signal<const mouse_info&> mouse_move;
        signal<const mouse_info&> mouse_down;
        signal<const mouse_info&> mouse_up;

    protected:
        // Setters and getters.
        virtual std::string get_title();
        virtual void set_title(std::string s);
        virtual size get_wsize();
        virtual void set_wsize(size sz);
        virtual pt get_location();
        virtual void set_location(pt location);
        virtual rct get_paint_area();

        // Pimpl. Concrete window must implement this!
        virtual native_wnd* native() = 0;
    };
//{{END.DEC}}

} // namespace nice

#endif // _WND_HPP