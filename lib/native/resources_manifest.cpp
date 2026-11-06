//
// Compiles a strict JSON resource manifest into owned pack entries.
// Relative assets resolve against the manifest, not the working directory.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native/resources.h>
#include "resource_io.h"
#include "resource_json.h"
#include <algorithm>
#include <initializer_list>
#include <stdexcept>

namespace
{
    using json = native::detail::resource_json;
    [[noreturn]] void invalid(const std::string &message) {
        throw std::invalid_argument("Resource manifest: " + message);
    }
    template<class type> const type &as(const json &value) {
        const auto *result = std::get_if<type>(&value.value);
        if (!result) invalid("wrong JSON value type");
        return *result;
    }
    void keys(const json::object &object,
              std::initializer_list<std::string_view> allowed) {
        for (const auto &[key, value] : object) {
            (void)value;
            if (std::find(allowed.begin(), allowed.end(), key) == allowed.end())
                invalid("unknown field " + key);
        }
    }
    const json &required(const json::object &object, const std::string &key) {
        auto found = object.find(key);
        if (found == object.end()) invalid("missing field " + key);
        return found->second;
    }
    std::string optional(const json::object &object, const std::string &key) {
        auto found = object.find(key);
        return found == object.end() ? "" : as<std::string>(found->second);
    }
}
namespace native
{
    resources resources::from_manifest(const std::filesystem::path &path) {
        const auto bytes = detail::read_resource_file(path, 1024 * 1024);
        const std::string text(bytes.begin(), bytes.end());
        const auto document = detail::parse_resource_json(text);
        const auto &root = as<json::object>(document);
        keys(root, {"version", "resources"});
        if (as<std::uint32_t>(required(root, "version")) != 1)
            invalid("unsupported version");
        resources result;
        std::size_t payload_size = 0;
        for (const auto &item : as<json::array>(required(root, "resources"))) {
            const auto &object = as<json::object>(item);
            keys(object, {"name", "type", "variants"});
            const auto name = as<std::string>(required(object, "name"));
            const auto type = as<std::string>(required(object, "type"));
            for (const auto &entry : result.entries())
                if (entry.name == name) invalid("duplicate resource " + name);
            const auto &variants = as<json::array>(required(object, "variants"));
            if (variants.empty()) invalid("empty variants for " + name);
            for (const auto &variant : variants) {
                const auto &fields = as<json::object>(variant);
                keys(fields, {"file", "when"});
                resource_variant value{name, type, {}, {}};
                if (auto it = fields.find("when"); it != fields.end()) {
                    const auto &when = as<json::object>(it->second);
                    keys(when, {"platform", "toolkit", "color", "theme", "size"});
                    value.when.platform = optional(when, "platform");
                    value.when.toolkit = optional(when, "toolkit");
                    value.when.color = optional(when, "color");
                    value.when.theme = optional(when, "theme");
                    if (auto size = when.find("size"); size != when.end())
                        value.when.size = as<std::uint32_t>(size->second);
                }
                const auto file = as<std::string>(required(fields, "file"));
                if (file.empty() || file.find('\0') != std::string::npos)
                    invalid("empty or NUL-containing asset path");
                value.data = detail::read_resource_file(
                    path.parent_path() / std::filesystem::u8path(file),
                    detail::resource_limit - payload_size);
                payload_size += value.data.size();
                result.add(std::move(value));
            }
        }
        // Enforce the same aggregate limits and defaults as binary output.
        (void)result.encode();
        return result;
    }
}
