//
// Implements the application event loop through the WINGs dispatcher.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <cerrno>
#include <fcntl.h>
#include <mutex>
#include <stdexcept>
#include <unistd.h>

#include <native/app.h>

#include "globals.h"
#include "../../post_backend.h"

namespace
{
    std::mutex posted_mutex;
    int posted_write = -1;

    // Wake WINGs without calling Xlib or the toolkit from a worker.
    void wake_posted_work() {
        std::lock_guard<std::mutex> guard(posted_mutex);
        if (posted_write < 0) return;
        const unsigned char byte = 1;
        while (write(posted_write, &byte, sizeof(byte)) < 0 && errno == EINTR) {}
        // A full nonblocking pipe already contains the required wakeup.
    }

    // Marshal a descriptor wake to an event; user callbacks run after dispatch.
    class posted_session
    {
    public:
        explicit posted_session(Display *display) : _display(display) {
            if (pipe(_descriptors) != 0)
                throw std::runtime_error("WINGs: cannot create event-loop pipe.");
            for (const int fd : _descriptors) {
                const int flags = fcntl(fd, F_GETFL, 0);
                if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0 ||
                    fcntl(fd, F_SETFD, FD_CLOEXEC) < 0) {
                    close(_descriptors[0]); close(_descriptors[1]);
                    throw std::runtime_error("WINGs: cannot configure event-loop pipe.");
                }
            }
            _atom = XInternAtom(display, "NATIVE_POSTED_WORK", False);
            _input = WMAddInputHandler(_descriptors[0], WIReadMask, receive, this);
            if (!_input) {
                close(_descriptors[0]); close(_descriptors[1]);
                throw std::runtime_error("WINGs: cannot register event-loop pipe.");
            }
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
            WMDeleteInputHandler(_input);
            close(_descriptors[0]); close(_descriptors[1]);
        }
        bool get_wake(const XEvent &event) const {
            return event.type == ClientMessage && event.xclient.message_type == _atom;
        }
    private:
        static void receive(int descriptor, int, void *data) {
            auto *self = static_cast<posted_session *>(data);
            unsigned char bytes[128];
            for (;;) {
                const auto count = read(descriptor, bytes, sizeof(bytes));
                if (count > 0 || (count < 0 && errno == EINTR)) continue;
                break;
            }
            XEvent event{};
            event.xclient.type = ClientMessage;
            event.xclient.display = self->_display;
            event.xclient.window = DefaultRootWindow(self->_display);
            event.xclient.message_type = self->_atom;
            event.xclient.format = 32;
            XPutBackEvent(self->_display, &event);
        }
        Display *_display;
        int _descriptors[2] = {-1, -1};
        Atom _atom = None;
        WMHandlerID _input = nullptr;
    };
}

namespace native
{
    int app::main_loop() {
        if (!linux::wmaker::initialized || !linux::wmaker::screen ||
            !linux::wmaker::display) {
            throw std::runtime_error(
                "Window Maker/WINGs: no application screen.");
        }

        linux::wmaker::exit_requested = false;
        {
            const posted_session posted(linux::wmaker::display);
            detail::drain_posted_work();
            while (!linux::wmaker::exit_requested) {
                XEvent event = {};
                WMNextEvent(linux::wmaker::display, &event);
                if (!posted.get_wake(event) && !linux::wmaker::handle_menu_event(event))
                    WMHandleEvent(&event);
                linux::wmaker::dispatch_deferred();
                detail::drain_posted_work();
            }
        }

        linux::wmaker::wnd_bindings.clear();
        linux::wmaker::window_bindings.clear();
        linux::wmaker::graphics_bindings.clear();
        if (linux::wmaker::list_selection_background) {
            WMReleaseColor(
                linux::wmaker::list_selection_background);
            linux::wmaker::list_selection_background = nullptr;
        }
        if (linux::wmaker::list_selection_text) {
            WMReleaseColor(linux::wmaker::list_selection_text);
            linux::wmaker::list_selection_text = nullptr;
        }
        linux::wmaker::screen = nullptr;
        linux::wmaker::display = nullptr;
        linux::wmaker::initialized = false;
        WMReleaseApplication();
        return 0;
    }
} // namespace native
