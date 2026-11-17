//
// Implements typed settings bindings, application-owned namespaces and
// monotonic per-binding read caches. All backend access is serialized within
// a process; file adapters additionally coordinate cooperating processes.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/configuration.h>
#include <native/app.h>

#include "configuration_backend.h"
#include "text_util.h"

#include <cmath>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <utility>

namespace native::detail
{
    struct configuration_binding
    {
        std::shared_ptr<configuration_backend> backend;
        std::string key;
        configuration_value fallback;
        std::chrono::milliseconds timeout;
        mutable std::mutex mutex;
        std::optional<configuration_value> cached;
        std::chrono::steady_clock::time_point loaded;

        configuration_binding(std::shared_ptr<configuration_backend> backend,
            std::string key, configuration_value fallback,
            std::chrono::milliseconds timeout)
            : backend(std::move(backend)), key(std::move(key)), fallback(std::move(fallback)),
              timeout(timeout) {}
    };

    void validate_configuration_name(const std::string &name) {
        if (name.empty() || name.size() > 255)
            throw std::invalid_argument("Invalid configuration identifier length.");
        bool segment = false;
        unsigned segments = 1;
        for (const char c : name) {
            if (c == '.') {
                if (!segment)
                    throw std::invalid_argument("Empty configuration identifier segment.");
                if (++segments > 16)
                    throw std::invalid_argument("Configuration identifiers allow at most 16 segments.");
                segment = false;
            } else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                       c == '-' || c == '_') {
                segment = true;
            } else {
                throw std::invalid_argument("Configuration identifiers must use lowercase ASCII.");
            }
        }
        if (!segment)
            throw std::invalid_argument("Empty configuration identifier segment.");
    }

    void validate_configuration_value(const configuration_value &value) {
        if (const auto *text = std::get_if<std::string>(&value)) {
            if (text->size() > configuration_string_limit ||
                text->find('\0') != std::string::npos || !valid_utf8(*text))
                throw std::invalid_argument("Configuration strings must be bounded UTF-8 without NUL.");
        }
        if (const auto *number = std::get_if<double>(&value);
            number && !std::isfinite(*number))
            throw std::invalid_argument("Configuration numbers must be finite.");
    }
}

namespace
{
    using native::detail::configuration_value;

    std::mutex &storage_mutex() {
        static std::mutex mutex;
        return mutex;
    }

    void validate_timeout(std::chrono::milliseconds timeout) {
        if (timeout.count() < 0)
            throw std::invalid_argument("Configuration timeout cannot be negative.");
    }

    // Store integers losslessly in the common signed 64-bit native domain.
    template<typename T>
    configuration_value encode(T value) {
        if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, bool>) {
            configuration_value result(std::move(value));
            native::detail::validate_configuration_value(result);
            return result;
        } else if constexpr (std::is_integral_v<T>) {
            if (!std::in_range<std::int64_t>(value))
                throw std::out_of_range("Configuration integer exceeds signed 64-bit range.");
            return static_cast<std::int64_t>(value);
        } else {
            configuration_value result(static_cast<double>(value));
            native::detail::validate_configuration_value(result);
            return result;
        }
    }

    template<typename T>
    T decode(const configuration_value &value) {
        native::detail::validate_configuration_value(value);
        if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, bool>) {
            if (const auto *stored = std::get_if<T>(&value))
                return *stored;
        } else if constexpr (std::is_integral_v<T>) {
            if (const auto *stored = std::get_if<std::int64_t>(&value)) {
                if (!std::in_range<T>(*stored))
                    throw std::out_of_range("Stored configuration integer is out of range.");
                return static_cast<T>(*stored);
            }
        } else {
            if (const auto *stored = std::get_if<double>(&value)) {
                if (std::abs(*stored) > std::numeric_limits<T>::max() ||
                    (*stored != 0 && std::abs(*stored) < std::numeric_limits<T>::denorm_min()))
                    throw std::out_of_range("Stored configuration number is out of range.");
                return static_cast<T>(*stored);
            }
        }
        throw std::runtime_error("Stored configuration value has the wrong type.");
    }
}

namespace native
{
    template<typename T>
    configuration<T>::configuration(std::string key, T fallback,
                                    std::chrono::milliseconds timeout) {
        detail::validate_configuration_name(key);
        validate_timeout(timeout);
        auto encoded = encode(std::move(fallback));
        auto backend = detail::make_configuration_backend(app::get_name());
        _binding = std::make_shared<detail::configuration_binding>(
            std::move(backend), std::move(key), std::move(encoded), timeout);
    }

    template<typename T>
    configuration<T>::configuration(const configuration &source) = default;

    template<typename T>
    configuration<T> &configuration<T>::operator=(const configuration &source) {
        if (_binding != source._binding)
            set_value(source.get_value());
        return *this;
    }

    template<typename T>
    configuration<T> &configuration<T>::operator=(T value) {
        return set_value(std::move(value));
    }

    template<typename T>
    T configuration<T>::read(bool force) const {
        auto &binding = *_binding;
        std::lock_guard lock(binding.mutex);
        const auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - binding.loaded);
        if (!force && binding.cached && age < binding.timeout)
            return decode<T>(*binding.cached);
        binding.cached.reset();
        std::lock_guard storage_lock(storage_mutex());
        auto value = binding.backend->read(binding.key);
        configuration_value result = value ? std::move(*value) : binding.fallback;
        T decoded = decode<T>(result);
        binding.cached = std::move(result);
        binding.loaded = std::chrono::steady_clock::now();
        return decoded;
    }

    template<typename T>
    T configuration<T>::get_value() const { return read(false); }

    template<typename T>
    configuration<T>::operator T() const { return get_value(); }

    template<typename T>
    T configuration<T>::reload() const { return read(true); }

    template<typename T>
    configuration<T> &configuration<T>::set_value(T value) {
        auto encoded = encode(std::move(value));
        auto &binding = *_binding;
        std::lock_guard lock(binding.mutex);
        std::lock_guard storage_lock(storage_mutex());
        binding.backend->write(binding.key, encoded);
        binding.cached = std::move(encoded);
        binding.loaded = std::chrono::steady_clock::now();
        return *this;
    }

    template<typename T>
    configuration<T> &configuration<T>::reset() {
        auto &binding = *_binding;
        std::lock_guard lock(binding.mutex);
        std::lock_guard storage_lock(storage_mutex());
        binding.backend->write(binding.key, std::nullopt);
        binding.cached.reset();
        return *this;
    }

    template<typename T>
    configuration<T> &configuration<T>::set_timeout(std::chrono::milliseconds timeout) {
        validate_timeout(timeout);
        std::lock_guard lock(_binding->mutex);
        _binding->timeout = timeout;
        _binding->cached.reset();
        return *this;
    }

    template<typename T>
    std::chrono::milliseconds configuration<T>::get_timeout() const {
        std::lock_guard lock(_binding->mutex);
        return _binding->timeout;
    }

    template<typename T>
    const std::string &configuration<T>::get_key() const { return _binding->key; }

    template class configuration<std::string>;
    template class configuration<bool>;
    template class configuration<short>;
    template class configuration<unsigned short>;
    template class configuration<int>;
    template class configuration<unsigned int>;
    template class configuration<long>;
    template class configuration<unsigned long>;
    template class configuration<long long>;
    template class configuration<unsigned long long>;
    template class configuration<float>;
    template class configuration<double>;
}
