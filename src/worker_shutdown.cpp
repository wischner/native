//
// Joins retired workers independently of window callbacks and UI turns.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "worker_shutdown.h"
#include <condition_variable>
#include <deque>
#include <mutex>

namespace
{
    class shutdown_controller
    {
    public:
        shutdown_controller()
            : _joiner([this](std::stop_token stop) { run(stop); }) {}
        ~shutdown_controller() {
            finish();
        }
        void retire(std::jthread worker) {
            worker.request_stop();
            if (!worker.joinable())
                return;
            {
                const std::lock_guard<std::mutex> lock(_mutex);
                _workers.push_back(std::move(worker));
            }
            _changed.notify_one();
        }
        void finish() {
            _joiner.request_stop();
            _changed.notify_all();
            if (_joiner.joinable())
                _joiner.join();
        }

    private:
        void run(std::stop_token stop) {
            for (;;) {
                std::jthread worker;
                {
                    std::unique_lock lock(_mutex);
                    _changed.wait(lock, stop,
                                  [this] { return !_workers.empty(); });
                    if (_workers.empty() && stop.stop_requested())
                        return;
                    worker = std::move(_workers.front());
                    _workers.pop_front();
                }
                if (worker.joinable())
                    worker.join();
            }
        }
        std::mutex _mutex;
        std::condition_variable_any _changed;
        std::deque<std::jthread> _workers;
        std::jthread _joiner;
    };
    shutdown_controller &controller() {
        static shutdown_controller value;
        return value;
    }
} // namespace
namespace vision
{
    void retire_worker(std::jthread worker) {
        controller().retire(std::move(worker));
    }
    worker_shutdown::~worker_shutdown() {
        controller().finish();
    }
} // namespace vision
