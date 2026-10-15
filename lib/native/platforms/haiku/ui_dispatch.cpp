//
// Owns the common Haiku UI gate and the initial creation transaction.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "ui_dispatch.h"
#include "../../ui_startup.h"
#include <memory>
namespace
{
    std::recursive_mutex mutex;
    std::unique_ptr<std::unique_lock<std::recursive_mutex>> startup;
} // namespace
namespace haiku
{
    std::recursive_mutex &ui_mutex() {
        return mutex;
    }
    ui_callback::ui_callback(BLooper &looper)
        : _gate(mutex, std::defer_lock) {
        const int32 locks = looper.CountLocks();
        for (int32 i = 0; i < locks; ++i)
            looper.Unlock();
        _gate.lock();
        for (int32 i = 0; i < locks; ++i)
            looper.Lock();
    }
    void ui_callback::release() {
        _gate.unlock();
    }
    void begin_ui_startup() {
        startup =
            std::make_unique<std::unique_lock<std::recursive_mutex>>(mutex);
    }
    void end_ui_startup() {
        startup.reset();
    }
} // namespace haiku

namespace native::detail
{
    void begin_ui_startup() {
        haiku::begin_ui_startup();
    }
    void end_ui_startup() {
        haiku::end_ui_startup();
    }
} // namespace native::detail
