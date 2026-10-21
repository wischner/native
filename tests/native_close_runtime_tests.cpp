//
// Sends a real native close request, verifies cancellation keeps the window
// alive, and then accepts a second request through the same portable policy.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native.h>
#include <chrono>
#include <iostream>
#include <memory>
#include <optional>
#include <thread>
#if defined(_WIN32)
#include "../lib/native/platforms/windows/globals.h"
#elif defined(__APPLE__)
#include "../lib/native/platforms/mac/globals.h"
#elif defined(__HAIKU__)
#include "../lib/native/platforms/haiku/globals.h"
#elif defined(NATIVE_CLOSE_WAYLAND) || defined(NATIVE_CLOSE_ELEMENTARY)
#include "../lib/native/toolkits/wayland/globals.h"
#elif defined(NATIVE_CLOSE_SDL2)
#include <SDL2/SDL.h>
#include "../lib/native/toolkits/sdl2/globals.h"
#elif defined(NATIVE_CLOSE_X11)
#include "../lib/native/toolkits/x11/globals.h"
#elif defined(NATIVE_CLOSE_MOTIF)
#include "../lib/native/toolkits/openmotif/globals.h"
#elif defined(NATIVE_CLOSE_OPENLOOK)
#include "../lib/native/toolkits/openlook/globals.h"
#elif defined(NATIVE_CLOSE_WMAKER)
#include "../lib/native/toolkits/wmaker/globals.h"
#elif defined(NATIVE_CLOSE_GEMIX)
#include "../lib/native/toolkits/gemix/globals.h"
#endif

namespace
{
    class close_window final : public native::app_wnd
    {
    public:
        close_window() : app_wnd("Native close policy", 60, 60, 240, 120) {}
        bool accept = false;
        bool failed = false;
        int requests = 0;
        void request_close() override {
            ++requests;
            if (accept)
                app_wnd::request_close();
        }
    };
    // Enter through the actual window-system close translation.
    void native_close(close_window &window) {
#if defined(_WIN32)
        PostMessageW(windows::wnd_bindings.handle_from_object(&window),
                     WM_CLOSE, 0, 0);
#elif defined(__APPLE__)
        [mac::wnd_bindings.handle_from_object(&window) performClose:nil];
#elif defined(__HAIKU__)
        haiku::wnd_bindings.handle_from_object(&window)->PostMessage(
            B_QUIT_REQUESTED);
#elif defined(NATIVE_CLOSE_WAYLAND) || defined(NATIVE_CLOSE_ELEMENTARY)
        gboolean handled = false;
        g_signal_emit_by_name(linux::wayland::state(window).widget, "close-request", &handled);
#elif defined(NATIVE_CLOSE_SDL2)
        SDL_Event event{};
        event.type = SDL_WINDOWEVENT;
        event.window.event = SDL_WINDOWEVENT_CLOSE;
        event.window.windowID = SDL_GetWindowID(
            linux::sdl2::wnd_bindings.handle_from_object(&window));
        SDL_PushEvent(&event);
#elif defined(NATIVE_CLOSE_GEMIX)
        WORD message[8] = {
            WM_CLOSED, linux::gemix::runtime.appl_id,
            0,         linux::gemix::wnd_bindings.handle_from_object(&window),
            0,         0,
            0,         0};
        appl_write(linux::gemix::runtime.appl_id, sizeof(message), message);
#else
        Display *display = nullptr;
        Window handle = None;
#if defined(NATIVE_CLOSE_X11)
        Widget shell = linux::x11::shell_bindings.handle_from_object(&window);
        display = XtDisplay(shell);
        handle = XtWindow(shell);
#elif defined(NATIVE_CLOSE_MOTIF)
        Widget shell =
            linux::openmotif::shell_bindings.handle_from_object(&window);
        display = XtDisplay(shell);
        handle = XtWindow(shell);
#elif defined(NATIVE_CLOSE_OPENLOOK)
        auto frame = linux::openlook::window_state(&window)->frame;
        display = linux::openlook::cached_display;
        handle = static_cast<Window>(xv_get(frame, XV_XID));
#elif defined(NATIVE_CLOSE_WMAKER)
        display = linux::wmaker::display;
        handle = WMWidgetXID(linux::wmaker::state(&window)->window);
#endif
        XEvent event{};
        event.xclient.type = ClientMessage;
        event.xclient.display = display;
        event.xclient.window = handle;
        event.xclient.message_type =
            XInternAtom(display, "WM_PROTOCOLS", False);
        event.xclient.format = 32;
        event.xclient.data.l[0] =
            XInternAtom(display, "WM_DELETE_WINDOW", False);
        event.xclient.data.l[1] = CurrentTime;
        XSendEvent(display, handle, False, NoEventMask, &event);
        XFlush(display);
#endif
    }
} // namespace
int program(int, char **) {
    close_window window;
    std::optional<native::ui_dispatch_scope> delivery;
    std::optional<std::jthread> ticker;
    window.on_wnd_create.connect([&] {
        delivery.emplace(window);
        native::app::post([&] { native_close(window); });
        ticker.emplace([sender = delivery->sender()](std::stop_token stop) {
            const auto deadline =
                std::chrono::steady_clock::now() + std::chrono::seconds(8);
            while (!stop.stop_requested()) {
                sender.post_latest([deadline](native::wnd &target) {
                    auto &window = static_cast<close_window &>(target);
                    if (window.requests == 1 && !window.accept) {
                        window.failed |= !window.get_created();
                        window.accept = true;
                        native_close(window);
                    } else if (window.requests > 2 ||
                               std::chrono::steady_clock::now() > deadline) {
                        window.failed = true;
                        window.destroy();
                    }
                });
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
        return false;
    });
    native::app::run(window);
    if (ticker)
        ticker->request_stop();
    if (delivery)
        delivery->close();
    const bool passed = !window.failed && window.accept &&
                        window.requests == 2 && !window.get_created();
    if (!passed)
        std::cerr << "Native close policy failed: requests="
                  << window.requests << '\n';
    return passed ? 0 : 1;
}
