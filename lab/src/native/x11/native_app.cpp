
//
// native_app.cpp
// 
// Application entry point & logic native X11 code.
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 17.05.2021   tstih
// 
#include "nice.hpp"

namespace nice {

//{{BEGIN.DEF}}
    app_id app::id() {
        return ::getpid();
    }

    bool app::is_primary_instance() {
        // Are we already primary instance? If not, try to become one.
        if (!primary_) {
            std::string aname = app::name();

            // Pid file needs to go to /var/run
            std::ostringstream pfname, pid;
            pfname << "/tmp/" << aname << ".pid";
            pid << nice::app::id() << std::endl;

            // Open, lock, and forget. Let the OS close and unlock.
            int pfd = ::open(pfname.str().c_str(), O_CREAT | O_RDWR, 0666);
            int rc = ::flock(pfd, LOCK_EX | LOCK_NB);
            primary_ = !(rc && EWOULDBLOCK == errno);
            if (primary_) {
                // Write our process id into the file.
                ::write(pfd, pid.str().c_str(), pid.str().length());
                return false;
            }
        }
        return primary_;
    }

    void app::run(const app_wnd& w) {

        // We have to cast the constness away to 
        // call non-const functions on window.
        auto& main_wnd=const_cast<app_wnd &>(w);

        // Show the window.
        main_wnd.show();

        // Flush it all.
        ::XFlush(instance_.display);

        // Main event loop.
        XEvent e;
        bool quit=false;
	    while ( !quit ) // Will be interrupted by the OS.
	    {
	      ::XNextEvent ( instance_.display,&e );
	      quit = native_wnd::global_wnd_proc(e);
	    }
    }
//{{END.DEF}}

}