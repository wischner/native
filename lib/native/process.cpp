//
// Implements portable child configuration, result access and monitor lifetime.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/process.h>
#include "process_backend.h"
#include <stdexcept>
#include <utility>

namespace native::detail
{
    process_peer::process_peer(process_config value) : config(std::move(value)) {}
}

namespace native
{
    process::process(process_config config)
        : _peer(std::make_unique<detail::process_peer>(std::move(config))) {
        if (_peer->config.executable.empty() || _peer->config.stop_timeout.count() < 0)
            throw std::invalid_argument("Invalid child configuration.");
    }
    process::~process() { request_stop(); wait(); }
    bool process::get_running() const { return _peer->running.load(); }
    void process::request_stop() { _peer->cancel.request_stop(); }
    void process::wait() { if (_peer->worker.joinable()) _peer->worker.join(); }
    process_result process::get_result() const {
        std::lock_guard guard(_peer->mutex);
        return _peer->result;
    }
}
