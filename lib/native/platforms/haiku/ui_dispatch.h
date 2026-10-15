//
// Serializes portable Haiku callbacks across the application/window loopers.
// Native window locks are acquired only after the common recursive UI gate.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <mutex>
#include <Looper.h>
namespace haiku
{
    // Acquire gate -> looper even when entered by a locked native callback.
    class ui_callback final
    {
    public:
        explicit ui_callback(BLooper &looper);
        // Release before native Quit(), which can exit a looper without C++
        // unwinding.
        void release();

    private:
        std::unique_lock<std::recursive_mutex> _gate;
    };
    // Return the common gate for all portable UI work.
    std::recursive_mutex &ui_mutex();
    // Hold the gate while startup creates and shows the initial windows.
    void begin_ui_startup();
    // Let window loopers dispatch after portable startup has completed.
    void end_ui_startup();
} // namespace haiku
