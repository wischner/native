//
// Paces image-shader invalidations with one shared stop-aware clock.
// Each view has at most one queued tick; dead resource generations are inert.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "shader_schedule.h"
#include <native/app.h>
#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace
{
    class shader_clock
    {
    public:
        shader_clock() : _worker([this](std::stop_token stop) { run(stop); }) {}
        ~shader_clock() { _worker.request_stop(); _condition.notify_all(); }
        void add(const std::shared_ptr<native::detail::shader_schedule_entry> &entry) {
            std::lock_guard lock(_mutex);
            _entries.push_back(entry);
            _condition.notify_all();
        }
    private:
        void run(std::stop_token stop) {
            std::unique_lock lock(_mutex);
            while (!stop.stop_requested()) {
                _condition.wait_for(lock, stop, std::chrono::milliseconds(16), [] { return false; });
                if (stop.stop_requested()) break;
                std::vector<std::shared_ptr<native::detail::shader_schedule_entry>> ready;
                std::erase_if(_entries, [](const auto &weak) { return weak.expired(); });
                for (const auto &weak : _entries)
                    if (auto entry = weak.lock(); entry && entry->enabled &&
                        !entry->pending.exchange(true)) ready.push_back(std::move(entry));
                lock.unlock();
                for (auto &entry : ready) native::app::post([entry] {
                    entry->pending = false;
                    const auto lifetime = entry->lifetime.lock();
                    if (entry->enabled && lifetime && lifetime->alive &&
                        entry->owner->get_created() && entry->owner->get_visible())
                        entry->owner->invalidate();
                });
                lock.lock();
            }
        }
        std::mutex _mutex;
        std::condition_variable_any _condition;
        std::vector<std::weak_ptr<native::detail::shader_schedule_entry>> _entries;
        std::jthread _worker;
    };
}
namespace native::detail
{
    std::shared_ptr<shader_schedule_entry> schedule_shader(
        wnd &owner, std::weak_ptr<wnd_lifetime> lifetime) {
        static shader_clock clock;
        auto entry = std::make_shared<shader_schedule_entry>();
        entry->owner = &owner;
        entry->lifetime = std::move(lifetime);
        clock.add(entry);
        return entry;
    }
}
