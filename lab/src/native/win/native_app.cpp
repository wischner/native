
//
// native_app.cpp
// 
// Application entry point & logic native Windows code.
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 05.05.2021   tstih
// 
#include "nice.hpp"

namespace nice {

//{{BEGIN.DEF}}
    app_id app::id() {
        return ::GetCurrentProcessId();
    }

    bool app::is_primary_instance() {
        // Are we already primary instance? If not, try to become one.
        if (!primary_) {
            std::string aname = app::name();
            // Create local mutex.
            std::ostringstream name;
            name << "Local\\" << aname;
            ::CreateMutex(0, FALSE, name.str().c_str());
            // We are primary instance.
            primary_ = !(::GetLastError() == ERROR_ALREADY_EXISTS);
        }
        return primary_;
    }

    void app::run(const app_wnd& w) {

        // We have to cast the constness away to 
        // call non-const functions on window.
        auto& main_wnd=const_cast<app_wnd &>(w);
        main_wnd.show();

        // Message loop.
        MSG msg;
        while (::GetMessage(&msg, NULL, 0, 0))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
        }

        // Finally, set the return code.
        ret_code = (int)msg.wParam;
    }
//{{END.DEF}}

}