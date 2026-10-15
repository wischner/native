//
// Declares off-callback worker retirement for Vision controllers.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <thread>
namespace vision
{
    // Request stop and transfer joining to the independent shutdown
    // controller.
    void retire_worker(std::jthread worker);
    // Drain retired workers after app::run returns, outside UI callbacks.
    class worker_shutdown final
    {
    public:
        ~worker_shutdown();
    };
} // namespace vision
