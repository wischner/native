//
// Qualifies real C++20 thread, mutex, cancellation and wake support before
// Native's shared UI delivery and shader scheduler can run on AmigaOS.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <condition_variable>
#include <mutex>
#include <stop_token>
#include <thread>

int main() {
    std::mutex mutex;
    std::condition_variable_any changed;
    bool ready = false;
    std::jthread worker([&](std::stop_token stop) {
        std::unique_lock lock(mutex);
        changed.wait(lock, stop, [&] { return ready; });
    });
    {
        const std::lock_guard lock(mutex);
        ready = true;
    }
    changed.notify_all();
    worker.join();
    std::jthread cancelled([&](std::stop_token stop) {
        std::unique_lock lock(mutex);
        changed.wait(lock, stop, [] { return false; });
    });
    cancelled.request_stop();
    cancelled.join();
    return 0;
}
