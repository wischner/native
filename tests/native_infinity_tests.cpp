//
// Tests physical key pairing and scoped UI lifetime without a native display.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>
#include "post_backend.h"
#include "input_state.h"
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>

namespace
{
    void require(bool value, const char *message) {
        if (!value) throw std::runtime_error(message);
    }
    class probe_window final : public native::wnd
    {
        void create_native() override {}
        void show_native() override {}
        void destroy_native() override {}
        void apply_cursor() override {}
    public:
        // Use a portable peer without requiring a display server in CTest.
        void prepare() {
            _created = true; _visible = true;
            _peer = native::detail::create_wnd_peer(*this);
            native::detail::assign_peer_state(*this, new native::detail::input_state);
        }
    };
    void keyboard() {
        probe_window window;
        window.prepare();
        int calls = 0, resets = 0;
        window.on_key.connect([&](native::key_event event) {
            ++calls;
            if (event.action == native::key_action::release)
                require(!event.repeat, "Repeated release.");
            return true;
        });
        window.on_key_reset.connect([&] { ++resets; return false; });
        require(!window.on_native_key({native::key_code::a, native::key_action::release}),
            "Unpaired release delivered.");
        require(!window.on_native_key({native::key_code::a, native::key_action::press, true}),
            "Unpaired repeat delivered.");
        require(window.on_native_key({native::key_code::a, native::key_action::press}),
            "Consumption not returned.");
        window.on_native_key({native::key_code::a, native::key_action::press, true});
        window.on_native_key({native::key_code::a, native::key_action::release, true});
        require(calls == 3, "Press/repeat/release mismatch.");
        window.on_native_key({native::key_code::left_shift, native::key_action::press});
        window.on_native_key({native::key_code::right_shift, native::key_action::press});
        window.on_native_focus(false);
        window.on_native_key_reset();
        require(resets == 1, "Cancellation not exactly once.");
        require(!native::detail::key_held(window, native::key_code::left_shift), "Held after reset.");
        require(native::detail::usb_key(4) == native::key_code::a, "USB position.");
        require(native::detail::pc_key(28, true) == native::key_code::keypad_enter,
            "Extended PC position.");
        window.destroy();
    }
    void dispatch() {
        // Use the public create lifecycle to obtain a lifetime token, with
        // an application window subclass whose native hooks are inert.
        class receiver final : public native::app_wnd {
            void create_native() override {}
            void show_native() override {}
            void destroy_native() override {}
            void apply_cursor() override {}
        public: receiver() : native::app_wnd("test") {}
        } window;
        window.create();
        native::ui_dispatch_scope scope(window, 2);
        auto sender = scope.sender();
        int count = 0, latest = -1;
        require(sender.post([&](auto &) { ++count; }) == native::ui_post_result::accepted,
            "FIFO admission.");
        sender.post([&](auto &) { ++count; });
        require(sender.post([](auto &) {}) == native::ui_post_result::full, "FIFO bound.");
        std::jthread producer([sender, &latest] {
            for (int i = 0; i < 10000; ++i)
                sender.post_latest([&latest, i](auto &) { latest = i; });
        });
        producer.join();
        native::detail::drain_posted_work();
        require(count == 2 && latest == 9999, "Coalescing / FIFO dispatch.");
        sender.post([&](auto &) { ++count; scope.close(); });
        sender.post([&](auto &) { ++count; });
        native::detail::drain_posted_work();
        require(count == 3, "Reentrant invalidation failed.");
        require(sender.post([](auto &) {}) == native::ui_post_result::closed, "Closed admission.");
        native::ui_dispatch_scope next(window);
        auto stale = next.sender();
        stale.post([&](auto &) { ++count; });
        window.destroy(); window.create();
        native::detail::drain_posted_work();
        require(count == 3, "Callback reached recreated receiver.");
        require(stale.post([](auto &) {}) == native::ui_post_result::closed, "Stale endpoint.");
        native::ui_dispatch_scope concurrent(window, 2);
        auto racing = concurrent.sender();
        std::atomic<bool> submitted = false;
        std::jthread racing_worker([racing, &submitted] {
            while (racing.post_latest([](auto &) {}) != native::ui_post_result::closed)
                submitted = true;
        });
        while (!submitted.load()) std::this_thread::yield();
        concurrent.close(); racing_worker.join();
        native::detail::drain_posted_work();
        native::ui_dispatch_scope reentrant(window);
        auto destroys = reentrant.sender();
        window.on_native_key({native::key_code::a, native::key_action::press});
        window.on_key_reset.connect([&] { window.destroy(); return false; });
        destroys.post([](auto &target) { target.destroy(); });
        destroys.post([&](auto &) { ++count; });
        native::detail::drain_posted_work();
        require(count == 3 && !window.get_created(), "Reentrant receiver destruction.");
        window.create();
        window.on_native_key({native::key_code::a, native::key_action::press});
        window.on_native_destroy();
        require(!window.get_created(), "Native-originated reentrant destruction.");
        window.destroy();
        native::detail::discard_posted_work();
    }
}
int main() {
    try { keyboard(); dispatch(); std::cout << "Infinity core contracts passed.\n"; }
    catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
}
