
//
// native_app.cpp
// 
// Application entry point & logic native SDL.
// 
// (c) 2021 Tomaz Stih
// This code is licensed under MIT license (see LICENSE.txt for details).
// 
// 02.06.2021   tstih
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

        // Show the main window.
        auto& main_wnd=const_cast<app_wnd &>(w);
        main_wnd.show();

        // Main event loop.
        SDL_Event e;
        /* Clean the queue */
        bool quit=false;
        while (!quit) { 
            // Wait for something to happen.
            SDL_WaitEvent(&e);
            quit = native_wnd::global_wnd_proc(e);
        }
    }
//{{END.DEF}}

}