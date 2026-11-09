//
// Serializes bounded little-endian resource packs without native structs,
// compression or filesystem extraction. Reads validate before returning.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native/resources.h>
#include "resource_io.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <map>
#include <stdexcept>

namespace native::detail
{
    std::vector<std::uint8_t> read_resource_file(
        const std::filesystem::path &path, std::size_t limit) {
        std::ifstream input(path, std::ios::binary | std::ios::ate);
        if (!input) throw std::runtime_error("Cannot open resource file: " + path.string());
        const auto size = input.tellg();
        if (size < 0 || static_cast<std::uint64_t>(size) > limit)
            throw std::invalid_argument("Resource file exceeds size limit: " + path.string());
        std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
        input.seekg(0);
        if (!bytes.empty())
            input.read(reinterpret_cast<char *>(bytes.data()), bytes.size());
        if (!input || input.peek() != std::char_traits<char>::eof() || input.bad())
            throw std::runtime_error("Cannot read resource file: " + path.string());
        return bytes;
    }
    void write_resource_file(const std::filesystem::path &path,
                             std::span<const std::uint8_t> bytes) {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output) throw std::runtime_error("Cannot create resource file: " + path.string());
        if (!bytes.empty())
            output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        output.close();
        if (!output) throw std::runtime_error("Cannot write resource file: " + path.string());
    }
}
namespace
{
    constexpr std::array<std::uint8_t, 8> magic{'N', 'R', 'E', 'S', '\r', '\n', 26, '\n'};
    [[noreturn]] void invalid() {
        throw std::invalid_argument("Malformed or unsupported Native resource pack.");
    }
    // Require a generic default per name, including packs made in C++.
    void defaults(const std::vector<native::resource_variant> &entries) {
        std::map<std::string, bool> found;
        for (const auto &entry : entries) {
            auto &present = found[entry.name];
            const auto &c = entry.when;
            present = present || (c.platform.empty() && c.toolkit.empty() &&
                c.color.empty() && c.theme.empty() && c.size == 0);
        }
        for (const auto &[name, present] : found)
            if (!present) throw std::invalid_argument("Missing default resource: " + name);
    }
    void number(std::vector<std::uint8_t> &out, std::uint32_t n) {
        for (int shift = 0; shift < 32; shift += 8)
            out.push_back(static_cast<std::uint8_t>(n >> shift));
    }
    void string(std::vector<std::uint8_t> &out, const std::string &text) {
        number(out, static_cast<std::uint32_t>(text.size()));
        out.insert(out.end(), text.begin(), text.end());
    }
    class reader
    {
    public:
        std::span<const std::uint8_t> bytes;
        std::size_t position = 0;
        std::span<const std::uint8_t> take(std::size_t count) {
            if (count > bytes.size() - position) invalid();
            auto result = bytes.subspan(position, count);
            position += count;
            return result;
        }
        std::uint32_t number() {
            const auto data = take(4);
            std::uint32_t value = 0;
            for (unsigned i = 0; i < 4; ++i)
                value |= std::uint32_t(data[i]) << (8 * i);
            return value;
        }
        std::string string() {
            auto length = number();
            if (length > 1024) invalid();
            auto data = take(length);
            return {data.begin(), data.end()};
        }
    };
}
namespace native
{
    resources resources::load(const std::filesystem::path &path) {
        return decode(detail::read_resource_file(path, detail::resource_limit));
    }
    resources resources::decode(std::span<const std::uint8_t> bytes) {
        if (bytes.size() > detail::resource_limit) invalid();
        reader input{bytes};
        const auto signature = input.take(magic.size());
        if (!std::equal(signature.begin(), signature.end(), magic.begin()) ||
            input.number() != 1) invalid();
        const auto count = input.number();
        if (count > 4096) invalid();
        resources result;
        for (std::uint32_t i = 0; i < count; ++i) {
            resource_variant entry;
            entry.name = input.string();
            entry.type = input.string();
            entry.when.platform = input.string();
            entry.when.toolkit = input.string();
            entry.when.color = input.string();
            entry.when.theme = input.string();
            entry.when.size = input.number();
            const auto data = input.take(input.number());
            entry.data.assign(data.begin(), data.end());
            result.add(std::move(entry));
        }
        if (input.position != bytes.size()) invalid();
        defaults(result.entries());
        return result;
    }
    std::vector<std::uint8_t> resources::encode() const {
        defaults(_entries);
        std::size_t total = 16;
        for (const auto &e : _entries) {
            total += 32 + e.name.size() + e.type.size() +
                e.when.platform.size() + e.when.toolkit.size() +
                e.when.color.size() + e.when.theme.size() + e.data.size();
            if (total > detail::resource_limit) invalid();
        }
        std::vector<std::uint8_t> out(magic.begin(), magic.end());
        out.reserve(total);
        number(out, 1);
        number(out, static_cast<std::uint32_t>(_entries.size()));
        for (const auto &e : _entries) {
            string(out, e.name);
            string(out, e.type);
            string(out, e.when.platform);
            string(out, e.when.toolkit);
            string(out, e.when.color);
            string(out, e.when.theme);
            number(out, e.when.size);
            number(out, static_cast<std::uint32_t>(e.data.size()));
            out.insert(out.end(), e.data.begin(), e.data.end());
        }
        return out;
    }
    void resources::save(const std::filesystem::path &path) const {
        detail::write_resource_file(path, encode());
    }
}
