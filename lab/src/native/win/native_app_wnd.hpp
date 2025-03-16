//
// native_app_wnd.hpp
// 
// Native application window declaration for MS Windows.
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 09.05.2021   tstih
// 
#ifndef _NATIVE_APP_WND_H
#define _NATIVE_APP_WND_H

#include "native_wnd.hpp"

namespace nice {

//{{BEGIN.DEC}}
    class app_wnd; // Forward declaration.
    class native_app_wnd : public native_wnd {
    public:
        native_app_wnd(
            app_wnd *window,
            std::string title,
            size size
        );
        virtual ~native_app_wnd();
        void show() const;
    };
//{{END.DEC}}

} // namespace nice

#endif // _NATIVE_APP_WND_H 