//
// Implements automatic signal disconnection for the public scoped
// connection handle.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/connection.h>

#include <utility>

namespace native
{
    connection::connection(std::function<void()> disconnect)
        : _disconnect(std::move(disconnect)) {}

    connection::connection(connection &&source) noexcept
        : _disconnect(std::move(source._disconnect)) {
        source._disconnect = nullptr;
    }

    connection &connection::operator=(connection &&source) noexcept {
        if (this == &source)
            return *this;
        if (_disconnect)
            _disconnect();
        _disconnect = std::move(source._disconnect);
        source._disconnect = nullptr;
        return *this;
    }

    connection::~connection() {
        if (_disconnect)
            _disconnect();
    }

    void connection::release() noexcept {
        _disconnect = nullptr;
    }

    connection::operator bool() const noexcept {
        return static_cast<bool>(_disconnect);
    }
} // namespace native
