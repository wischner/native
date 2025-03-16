
//
// app.hpp
// 
// Class encapsulating application entry point.
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 05.05.2021   tstih
// 
#ifndef _APP_HPP
#define _APP_HPP

#include "app_wnd.hpp"

namespace nice {

//{{BEGIN.DEC}}
    class app {
    public:
        // Cmd line arguments.
        static int argc;
        static char **argv;

        // Return code.
        static int ret_code;

        // Application (process) id.
        static app_id id();

        // Application name. First cmd line arg without extension.
        static std::string name();

        // Application instance get and set.
        static app_instance instance();
        static void instance(app_instance instance);

        // Is another instance already running?
        static bool is_primary_instance();

        // Main desktop application loop.
        static void run(const app_wnd& w);

    private:
        static bool primary_;
        static app_instance instance_;     
    };
//{{END.DEC}}
}

#endif // _APP_HPP