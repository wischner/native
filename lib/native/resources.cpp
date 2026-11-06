//
// Validates named resource variants and ranks compatible representations.
// Selection is portable and never inspects toolkit handles.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native/resources.h>
#include <algorithm>
#include <stdexcept>
#include <tuple>

namespace
{
    bool label(const std::string &text, bool optional = false) {
        if (text.empty()) return optional;
        if (text.size() > 1024) return false;
        return std::all_of(text.begin(), text.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-';
        });
    }
    auto selectors(const native::resource_context &c) {
        return std::tie(c.platform, c.toolkit, c.color, c.theme, c.size);
    }
    int match(const std::string &value, const std::string &wanted) {
        if (value.empty()) return 1;
        return value == wanted ? 2 : 0;
    }
}
namespace native
{
    void resources::add(resource_variant value) {
        const auto &c = value.when;
        if (!label(value.name) || !label(value.type) ||
            !label(c.platform, true) || !label(c.toolkit, true) ||
            (!c.color.empty() && c.color != "color" && c.color != "monochrome") ||
            (!c.theme.empty() && c.theme != "light" && c.theme != "dark") ||
            c.size > 65535 || value.data.size() > 64 * 1024 * 1024 ||
            _entries.size() >= 4096)
            throw std::invalid_argument("Invalid resource metadata or size.");
        auto bytes = [](const resource_variant &entry) {
            return std::size_t(32) + entry.name.size() + entry.type.size() +
                entry.when.platform.size() + entry.when.toolkit.size() +
                entry.when.color.size() + entry.when.theme.size() + entry.data.size();
        };
        std::size_t total = 16 + bytes(value);
        for (const auto &entry : _entries) {
            total += bytes(entry);
            if (entry.name != value.name) continue;
            if (entry.type != value.type)
                throw std::invalid_argument("Mixed resource types: " + value.name);
            if (selectors(entry.when) == selectors(c))
                throw std::invalid_argument("Duplicate resource variant: " + value.name);
        }
        if (total > 64 * 1024 * 1024)
            throw std::invalid_argument("Resource pack exceeds 64 MiB.");
        _entries.push_back(std::move(value));
    }
    const std::vector<resource_variant> &resources::entries() const {
        return _entries;
    }
    const resource_variant &resources::get(std::string_view name) const {
        return get(name, current_context());
    }
    const resource_variant &resources::get(
        std::string_view name, const resource_context &context) const {
        const resource_variant *best = nullptr;
        // Color compatibility precedes platform specificity. Size distance
        // uses signed wide arithmetic; larger wins an equal-distance tie.
        using rank = std::tuple<int, int, int, int, int, std::int64_t,
                                std::uint32_t>;
        rank best_rank{};
        for (const auto &entry : _entries) {
            if (entry.name != name) continue;
            const auto &c = entry.when;
            if ((!c.platform.empty() && c.platform != context.platform) ||
                (!c.toolkit.empty() && c.toolkit != context.toolkit)) continue;
            const auto distance = std::int64_t(c.size) - context.size;
            const rank score{
                match(c.color, context.color),
                match(c.toolkit, context.toolkit),
                match(c.platform, context.platform),
                match(c.theme, context.theme),
                context.size ? (c.size ? 1 : 0) : (c.size ? 0 : 1),
                -(distance < 0 ? -distance : distance), c.size};
            if (!best || score > best_rank) {
                best = &entry;
                best_rank = score;
            }
        }
        if (!best) throw std::out_of_range("Resource not found: " + std::string(name));
        return *best;
    }
}
