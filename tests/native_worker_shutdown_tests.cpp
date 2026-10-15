//
// Verifies that worker retirement returns before a blocked worker finishes,
// retains owned service state, and completes without another UI dispatch.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "../src/worker_shutdown.h"
#include <condition_variable>
#include <mutex>
#include <iostream>
#include <memory>
#include <stdexcept>

int main() {
    try {
        std::weak_ptr<int> lifetime;
        std::mutex mutex;
        std::condition_variable changed;
        bool release = false, started = false;
        {
            const vision::worker_shutdown shutdown;
            auto service = std::make_shared<int>(42);
            lifetime = service;
            std::jthread worker([service, &release, &started, &mutex,
                                 &changed](std::stop_token) {
                std::unique_lock lock(mutex);
                started = true;
                changed.notify_all();
                changed.wait(lock, [&] { return release; });
            });
            {
                std::unique_lock lock(mutex);
                changed.wait(lock, [&] { return started; });
            }
            vision::retire_worker(std::move(worker));
            service.reset();
            if (lifetime.expired())
                throw std::runtime_error(
                    "service released before worker finished");
            // This statement must be reachable before the worker can finish.
            {
                const std::lock_guard<std::mutex> lock(mutex);
                release = true;
            }
            changed.notify_all();
        }
        if (!lifetime.expired())
            throw std::runtime_error("shutdown did not join retired worker");
        std::cout << "Independent worker retirement passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
