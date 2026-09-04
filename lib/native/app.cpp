//
// Implements backend-neutral application startup state, and the queue
// that carries work from worker threads onto the user interface
// thread.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/app.h>
#include <native/app_wnd.h>
#include <native/screen.h>
#include <native/wnd.h>

#include "post_backend.h"

#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace native
{
    namespace
    {
        // Work queued by app::post, and the lock that guards it.
        //
        // A plain mutex rather than a lock free queue. Posting happens
        // when something arrives from outside the process, which for
        // any realistic application is rare next to the loop's own
        // rate, so there is no contention to design around and
        // clarity is worth more.
        std::mutex posted_mutex;
        std::vector<std::function<void()>> posted_work;

        // Backend routine that wakes a blocked event loop, or null
        // when the backend polls and needs no waking.
        void (*loop_wake)() = nullptr;
    } // namespace

    int app::argc = 0;
    char **app::argv = nullptr;
    char **app::envp = nullptr;
    app_wnd *app::_main_wnd = nullptr;

    int app::run(app_wnd &wnd) {
        if (_main_wnd)
            throw std::logic_error(
                "An application event loop is already active.");
        if (wnd.get_owner())
            throw std::invalid_argument(
                "An owned window cannot be the application main "
                "window.");

        _main_wnd = &wnd;

        try {
            // Populate screens before creation so handlers may query
            // them from the window's create signal.
            screen::detect();
            wnd.create();
            wnd.show();

            const int result = app::main_loop();

            // Work posted but not yet drained refers to windows that
            // are about to go. Drop it rather than run it against
            // them.
            detail::discard_posted_work();

            wnd.destroy();
            _main_wnd = nullptr;
            return result;
        } catch (...) {
            detail::discard_posted_work();
            wnd.destroy();
            _main_wnd = nullptr;
            throw;
        }
    }

    void app::post(std::function<void()> work) {
        if (!work)
            return;

        void (*wake)() = nullptr;
        {
            std::lock_guard<std::mutex> guard(posted_mutex);
            posted_work.push_back(std::move(work));
            wake = loop_wake;
        }

        // Wake outside the lock. A backend's wake routine takes its
        // own locks or writes to a pipe, and holding ours across that
        // is how deadlocks are built.
        if (wake)
            wake();
    }

    app_wnd *app::main_wnd() {
        return _main_wnd;
    }

    namespace detail
    {
        void drain_posted_work() {
            std::vector<std::function<void()>> ready;
            {
                std::lock_guard<std::mutex> guard(posted_mutex);
                if (posted_work.empty())
                    return;
                ready.swap(posted_work);
            }

            // Run outside the lock, so work that posts more work does
            // not deadlock on itself and a worker thread can keep
            // queueing while the loop is busy.
            for (std::function<void()> &work : ready)
                work();
        }

        void set_loop_wake(void (*wake)()) {
            std::lock_guard<std::mutex> guard(posted_mutex);
            loop_wake = wake;
        }

        void discard_posted_work() {
            std::vector<std::function<void()>> dropped;
            {
                std::lock_guard<std::mutex> guard(posted_mutex);
                dropped.swap(posted_work);
                loop_wake = nullptr;
            }
            // Destroyed outside the lock: a captured payload's
            // destructor is user code and must not run under it.
        }
    } // namespace detail
} // namespace native
