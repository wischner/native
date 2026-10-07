//
// Implements bounded FIFO and coalesced worker delivery with receiver invalidation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/ui_dispatch.h>
#include <native/app.h>
#include <native/wnd.h>
#include "input_state.h"
#include <deque>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace native::detail
{
    struct ui_dispatch_state
    {
        std::mutex mutex;
        std::deque<std::function<void(wnd &)>> fifo;
        std::function<void(wnd &)> latest;
        std::weak_ptr<wnd_lifetime> lifetime;
        wnd *receiver = nullptr;
        std::size_t capacity = 0;
        bool pending = false;
        bool open = true;
    };
}

namespace
{
    using state_type = native::detail::ui_dispatch_state;
    using work_type = std::function<void(native::wnd &)>;

    bool live(const state_type &state) {
        auto token = state.lifetime.lock();
        return state.open && token && token->alive.load();
    }

    void dispatch(const std::shared_ptr<state_type> &state);

    void wake(const std::shared_ptr<state_type> &state) {
        try {
            native::app::post([state] { dispatch(state); });
        } catch (...) {
            std::lock_guard guard(state->mutex);
            state->pending = false;
            throw;
        }
    }

    // A bounded turn leaves time for native input and painting.
    void dispatch(const std::shared_ptr<state_type> &state) {
        for (unsigned index = 0; index < 16; ++index) {
            work_type work;
            native::wnd *receiver = nullptr;
            {
                std::lock_guard guard(state->mutex);
                if (!live(*state)) { state->pending = false; return; }
                receiver = state->receiver;
                if (index == 0 && state->latest) {
                    work = std::move(state->latest);
                } else if (!state->fifo.empty()) {
                    work = std::move(state->fifo.front());
                    state->fifo.pop_front();
                } else if (state->latest) {
                    work = std::move(state->latest);
                } else { state->pending = false; return; }
            }
            try {
                work(*receiver);
            } catch (...) {
                // Allow a future submission to rearm after loop failure.
                std::lock_guard guard(state->mutex);
                state->pending = false;
                throw;
            }
        }
        wake(state);
    }

    native::ui_post_result submit(
        const std::weak_ptr<state_type> &weak, work_type work,
        bool latest) {
        auto state = weak.lock();
        if (!state || !work) return native::ui_post_result::closed;
        work_type replaced;
        bool should_wake = false;
        {
            std::lock_guard guard(state->mutex);
            if (!live(*state)) return native::ui_post_result::closed;
            if (latest) {
                replaced = std::move(state->latest);
                state->latest = std::move(work);
            } else {
                if (state->fifo.size() >= state->capacity)
                    return native::ui_post_result::full;
                state->fifo.push_back(std::move(work));
            }
            if (!state->pending) { state->pending = true; should_wake = true; }
        }
        if (should_wake) wake(state);
        return native::ui_post_result::accepted;
    }
}

namespace native
{
    ui_sender::ui_sender() = default;
    ui_post_result ui_sender::post(std::function<void(wnd &)> work) const {
        return submit(_state, std::move(work), false);
    }
    ui_post_result ui_sender::post_latest(
        std::function<void(wnd &)> work) const {
        return submit(_state, std::move(work), true);
    }
    ui_dispatch_scope::ui_dispatch_scope(wnd &receiver, std::size_t capacity)
        : _state(std::make_shared<detail::ui_dispatch_state>()) {
        if (!capacity) throw std::invalid_argument("Empty UI queue.");
        if (!receiver.get_created() || !receiver._lifetime)
            throw std::logic_error("UI receiver must be created.");
        _state->receiver = &receiver;
        _state->lifetime = receiver._lifetime;
        _state->capacity = capacity;
    }
    ui_dispatch_scope::~ui_dispatch_scope() { close(); }
    ui_sender ui_dispatch_scope::sender() const {
        ui_sender result;
        result._state = _state;
        return result;
    }
    void ui_dispatch_scope::close() {
        std::deque<std::function<void(wnd &)>> dropped;
        std::function<void(wnd &)> latest;
        {
            std::lock_guard guard(_state->mutex);
            _state->open = false;
            _state->receiver = nullptr;
            dropped.swap(_state->fifo);
            latest = std::move(_state->latest);
        }
        // Captured application destructors must run outside the mutex.
    }
}
