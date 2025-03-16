//
// app_wnd.hpp
// 
// Application window class. 
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 09.05.2021   tstih
// 
#ifndef _APP_WND_HPP
#define _APP_WND_HPP

#include "includes.hpp"
#include "resource.hpp"
#include "property.hpp"
#include "signal.h"
#include "geometry.hpp"

namespace nice {

//{{BEGIN.DEC}}
    class app_wnd : public wnd {
    public:
        app_wnd(std::string title, size size) : native_(nullptr) {
            // Store parameters.
            title_ = title; size_ = size;
            // Subscribe to destroy signal.
            destroyed.connect(this, &app_wnd::on_destroy);
        }
        void show();
        
    protected:
        // Destroyed handler...
        bool on_destroy();         
        // Pimpl implementation.
        virtual native_app_wnd* native() override;
    private:
        std::string title_;
        size size_;
        std::unique_ptr<native_app_wnd> native_;
    };
//{{END.DEC}}

} // namespace nice

#endif // _APP_WND_HPP