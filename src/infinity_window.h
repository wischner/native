//
// Declares Vision acceptance diagnostics for physical input and background services.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once
#include <native.h>
#include <memory>
#include <set>
#include <thread>
#include <string>

namespace vision
{
    // Portable manual acceptance surface; all operations use public Native APIs.
    class infinity_window final : public native::app_wnd
    {
    public:
        // Construct keyboard, PCM, worker and child-process diagnostics.
        infinity_window();
        // Stop and join bounded workers before releasing their services.
        ~infinity_window() override;
        // Cancel receiver work immediately on native resource destruction.
        void on_native_destroy() override;
    private:
        native::text_edit _editor;
        native::button _tone, _work, _cancel, _child, _modal_button;
        native::modal_wnd _modal;
        native::button _modal_close;
        native::audio_out _audio{44100, 2, 22050};
        std::unique_ptr<native::ui_dispatch_scope> _delivery;
        std::jthread _worker, _sound;
        std::set<native::key_code> _held;
        std::string _last = "Click the background and press physical keys.";
        std::string _status = "Ready.";
        unsigned _presses = 0, _releases = 0, _repeats = 0, _resets = 0;
        // Create child controls and bind the receiver scope.
        bool create_children();
        // Draw live keyboard state and service results.
        bool paint(native::wnd_paint_event event);
        // Run a bounded coalesced progress worker, ending with FIFO completion.
        void start_worker();
        // Generate an audible stereo tone on a producer thread.
        void start_tone();
        // Run this executable's deterministic helper on a monitor controller.
        void start_child();
        // Invalidate delivery and join cancellable workers on teardown.
        void stop();
    };
}
