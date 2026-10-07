//
// Implements the OpenMotif application event-loop backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <cerrno>
#include <fcntl.h>
#include <mutex>
#include <stdexcept>
#include <unistd.h>

#include <X11/Intrinsic.h>

#include <native.h>
#include <native/app.h>

#include "globals.h"
#include "../../post_backend.h"

namespace
{
    std::mutex posted_mutex;
    int posted_write = -1;

    // Wake Xt without calling Xlib or Xt from a posting worker thread.
    void wake_posted_work() {
        std::lock_guard<std::mutex> guard(posted_mutex);
        if (posted_write < 0) return;
        const unsigned char byte = 1;
        while (write(posted_write, &byte, sizeof(byte)) < 0 && errno == EINTR) {}
        // A full nonblocking pipe already contains the required wakeup.
    }

    // Consume wake bytes; queued C++ callbacks run after Xt dispatch returns.
    void receive_posted_work(XtPointer, int *descriptor, XtInputId *) {
        unsigned char bytes[128];
        for (;;) {
            const auto count = read(*descriptor, bytes, sizeof(bytes));
            if (count > 0 || (count < 0 && errno == EINTR)) continue;
            break;
        }
    }

    // Bound the Xt input registration and thread-safe wake target to one loop.
    class posted_session
    {
    public:
        explicit posted_session(XtAppContext context) {
            if (pipe(_descriptors) != 0)
                throw std::runtime_error("Motif: cannot create event-loop pipe.");
            for (const int fd : _descriptors) {
                const int flags = fcntl(fd, F_GETFL, 0);
                if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0 ||
                    fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
                    close(_descriptors[0]); close(_descriptors[1]);
                    throw std::runtime_error("Motif: cannot configure event-loop pipe.");
                }
            }
            _input = XtAppAddInput(context, _descriptors[0],
                reinterpret_cast<XtPointer>(XtInputReadMask), receive_posted_work, nullptr);
            {
                std::lock_guard<std::mutex> guard(posted_mutex);
                posted_write = _descriptors[1];
            }
            native::detail::set_loop_wake(wake_posted_work);
        }

        ~posted_session() {
            native::detail::set_loop_wake(nullptr);
            std::lock_guard<std::mutex> guard(posted_mutex);
            posted_write = -1;
            XtRemoveInput(_input);
            close(_descriptors[0]); close(_descriptors[1]);
        }

    private:
        int _descriptors[2] = {-1, -1};
        XtInputId _input = 0;
    };
}

namespace native
{

    int app::main_loop() {
        if (!linux::openmotif::app_instance)
            throw std::runtime_error("Motif: No Xt application context "
                                     "available for main loop.");

        linux::openmotif::exit_requested = false;
        {
            const posted_session posted(linux::openmotif::app_instance);
            detail::drain_posted_work();
            while (!linux::openmotif::exit_requested) {
                XtAppProcessEvent(linux::openmotif::app_instance, XtIMAll);
                detail::drain_posted_work();
            }
        }

        linux::openmotif::wnd_bindings.clear();
        linux::openmotif::shell_bindings.clear();
        linux::openmotif::wnd_gpx_bindings.clear();

        if (linux::openmotif::app_instance) {
            XtDestroyApplicationContext(linux::openmotif::app_instance);
            linux::openmotif::app_instance = nullptr;
        }

        linux::openmotif::cached_display = nullptr;
        linux::openmotif::wm_delete_window_atom = None;
        return 0;
    }

} // namespace native
