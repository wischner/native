//
// Implements the application event loop with the XView notifier so
// native OPEN LOOK controls retain their standard event translations.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <cerrno>
#include <fcntl.h>
#include <stdexcept>
#include <sys/select.h>
#include <unistd.h>

#include <native.h>
#include <native/app.h>

#include <xview/xview.h>
#include <xview/notify.h>
#include <xview/window.h>

#include "globals.h"
#include "../../post_backend.h"

#if defined(__sun)
// The Tribblix XView library exports its notifier-aware select wrapper, but
// an executable linked by modern GCC can make the wrapper's internal
// notify_select call resolve back to that same wrapper. Native applications
// do not call select outside XView's event loop, so route the internal call
// to the illumos libc entry point just as the bundled XView clients do.
extern "C" int _select(
    int,
    fd_set *,
    fd_set *,
    fd_set *,
    timeval *);

extern "C" int select(
    int nfds,
    fd_set *readfds,
    fd_set *writefds,
    fd_set *exceptfds,
    timeval *timeout) {
    return _select(nfds, readfds, writefds, exceptfds, timeout);
}
#endif

namespace
{
    int posted_pipe[2] = {-1, -1};

    void wake_posted_work() {
        if (posted_pipe[1] < 0)
            return;
        const unsigned char byte = 1;
        while (write(posted_pipe[1], &byte, sizeof(byte)) < 0 &&
               errno == EINTR) {
        }
    }

    Notify_value receive_posted_work(Notify_client, int fd) {
        unsigned char bytes[64];
        while (read(fd, bytes, sizeof(bytes)) > 0) {
        }
        native::detail::drain_posted_work();
        return NOTIFY_DONE;
    }

    void close_posted_pipe() {
        native::detail::set_loop_wake(nullptr);
        for (int &fd : posted_pipe) {
            if (fd >= 0)
                close(fd);
            fd = -1;
        }
    }
} // namespace

namespace native
{
    int app::main_loop() {
        if (!linux::openlook::initialized ||
            !linux::openlook::main_frame) {
            throw std::runtime_error(
                "OpenLook/XView: no main frame is available.");
        }

        linux::openlook::exit_requested = false;
        if (pipe(posted_pipe) != 0)
            throw std::runtime_error(
                "OpenLook/XView: unable to create event-loop pipe.");
        for (const int fd : posted_pipe) {
            const int flags = fcntl(fd, F_GETFL, 0);
            if (flags >= 0)
                fcntl(fd, F_SETFL, flags | O_NONBLOCK);
        }
        notify_set_input_func(
            linux::openlook::main_frame,
            reinterpret_cast<Notify_func>(receive_posted_work),
            posted_pipe[0]);
        detail::set_loop_wake(wake_posted_work);
        wake_posted_work();

        try {
            xv_main_loop(linux::openlook::main_frame);
        } catch (...) {
            close_posted_pipe();
            throw;
        }
        close_posted_pipe();

        linux::openlook::wnd_bindings.clear();
        linux::openlook::frame_bindings.clear();
        linux::openlook::window_bindings.clear();
        linux::openlook::wnd_gpx_bindings.clear();
        linux::openlook::main_frame = XV_NULL;
        return 0;
    }
} // namespace native
