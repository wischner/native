//
// Implements the X11 application event loop through Xt so Athena
// widgets receive their standard translations, callbacks, and popup
// behavior.
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
                throw std::runtime_error("X11/Athena: cannot create event-loop pipe.");
            for (const int fd : _descriptors) {
                const int flags = fcntl(fd, F_GETFL, 0);
                if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0 ||
                    fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
                    close(_descriptors[0]); close(_descriptors[1]);
                    throw std::runtime_error("X11/Athena: cannot configure event-loop pipe.");
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
        if (!linux::x11::app_instance)
            throw std::runtime_error(
                "X11/Athena: No Xt application context.");

        linux::x11::exit_requested = false;
        {
            const posted_session posted(linux::x11::app_instance);
            detail::drain_posted_work();
            while (!linux::x11::exit_requested) {
                XtAppProcessEvent(linux::x11::app_instance, XtIMAll);
                detail::drain_posted_work();
            }
        }

        linux::x11::wnd_bindings.clear();
        linux::x11::shell_bindings.clear();
        linux::x11::main_wnd_bindings.clear();
        linux::x11::wnd_gpx_bindings.clear();

        if (linux::x11::app_instance) {
            XtDestroyApplicationContext(linux::x11::app_instance);
            linux::x11::app_instance = nullptr;
        }

        linux::x11::cached_display = nullptr;
        linux::x11::wm_delete_window_atom = None;
        return 0;
    }
} // namespace native
