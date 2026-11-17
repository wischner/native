//
// Declares typed persistent settings with platform-owned storage and a
// per-binding read cache. Values are returned by copy, never by reference
// into a mutable cache. Template implementations are explicitly instantiated
// in the library for the supported scalar and UTF-8 string types.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <chrono>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>

namespace native
{
    namespace detail
    {
        struct configuration_binding;

        template<typename T>
        inline constexpr bool configuration_type =
            std::is_same_v<T, std::string> || std::is_same_v<T, bool> ||
            std::is_same_v<T, short> || std::is_same_v<T, unsigned short> ||
            std::is_same_v<T, int> || std::is_same_v<T, unsigned int> ||
            std::is_same_v<T, long> || std::is_same_v<T, unsigned long> ||
            std::is_same_v<T, long long> ||
            std::is_same_v<T, unsigned long long> ||
            std::is_same_v<T, float> || std::is_same_v<T, double>;
    }

    template<typename T = std::string>
    class configuration final
    {
        static_assert(detail::configuration_type<T>,
            "configuration supports strings, bool, integer and float/double values");

    public:
        // Bind a dotted key in app::get_name()'s preference store. A missing
        // value returns fallback without persisting it. Zero timeout reads
        // storage on every access; a positive timeout caches successful reads.
        // Construction validates arguments but does not read settings.
        explicit configuration(std::string key, T fallback = T{},
            std::chrono::milliseconds timeout = std::chrono::milliseconds{0});

        // Copy a handle to the same binding, including its shared read cache.
        configuration(const configuration &source);

        // Read source and persist its value to this key; never rebind this
        // handle. Assignment from a temporary has the same semantics.
        configuration &operator=(const configuration &source);

        // Persist a value immediately and refresh this binding's cache.
        // Invalid values and storage failures throw; destruction never writes.
        configuration &operator=(T value);

        // Read a value by copy. Missing keys use the default; malformed data,
        // type mismatch, out-of-range values and I/O errors throw exceptions.
        T get_value() const;

        // Implicitly read as T, with the same cache/error policy as get_value.
        operator T() const;

        // Persist a value; equivalent to assignment, with fluent chaining.
        configuration &set_value(T value);

        // Bypass the cache and return a fresh value. A failed read throws and
        // is never substituted with a stale cached value.
        T reload() const;

        // Remove this user's stored value and invalidate this binding's cache.
        // The next read uses platform defaults or the constructor fallback.
        configuration &reset();

        // Change cache duration and invalidate the cache; negative values throw.
        configuration &set_timeout(std::chrono::milliseconds timeout);

        // Return the cache duration; zero means no read caching.
        std::chrono::milliseconds get_timeout() const;

        // Return the immutable dotted key identifying this binding.
        const std::string &get_key() const;

        // Stream the current value. This tiny template adapter also supports
        // string bindings where ostream's own template deduction would not.
        friend std::ostream &operator<<(std::ostream &out,
                                       const configuration &value) {
            return out << value.get_value();
        }

    private:
        T read(bool force) const;
        std::shared_ptr<detail::configuration_binding> _binding;
    };

    // String literals select owning UTF-8 strings, never pointer-valued settings.
    configuration(std::string, const char *, std::chrono::milliseconds = {})
        -> configuration<std::string>;
    configuration(std::string) -> configuration<std::string>;
}
