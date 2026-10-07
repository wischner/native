//
// Holds portable child configuration, results and monitor ownership behind the public peer.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once
#include <native/process.h>
#include <atomic>
#include <mutex>
#include <thread>

namespace native::detail
{
    struct process_peer
    {
        process_config config;
        mutable std::mutex mutex;
        process_result result;
        std::atomic<bool> running{false};
        std::jthread worker;
        std::stop_source cancel;
        bool started = false;
        // Cache shared state; platform launch/monitor code owns native resources.
        explicit process_peer(process_config value);
    };
}
