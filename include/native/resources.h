//
// Owns portable resource variants and selects named assets using a
// backend profile. Packs support file and embedded-memory round trips.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace native
{
    // Empty strings and zero size mean unspecified. Color is "color"
    // or "monochrome"; theme is "light" or "dark" when specified.
    struct resource_context
    {
        std::string platform;
        std::string toolkit;
        std::string color;
        std::string theme;
        std::uint32_t size = 0;
    };

    // One representation. All entries sharing a name share one type.
    // Payload bytes are opaque; "icon" conventionally contains a PNG.
    struct resource_variant
    {
        std::string name;
        std::string type;
        resource_context when;
        std::vector<std::uint8_t> data;
    };

    class resources final
    {
    public:
        // Return the compiled backend profile, without opening a display.
        // X11/GEMix use monochrome; theme and pixel size are unspecified.
        static resource_context current_context();

        // Append a variant; reject invalid metadata, mixed types and
        // duplicate selectors with std::invalid_argument. Copies own data.
        void add(resource_variant value);

        // Inspect entries in insertion order. References survive queries,
        // but may be invalidated by add(), assignment or destruction.
        const std::vector<resource_variant> &entries() const;

        // Find the closest variant using current_context(); throw
        // std::out_of_range if the name does not exist.
        const resource_variant &get(std::string_view name) const;

        // Find using an explicit context. Foreign platform/toolkit variants
        // are excluded; deterministic ranking and default fallback apply.
        // Throws out_of_range if no eligible representation exists.
        const resource_variant &get(std::string_view name,
                                    const resource_context &context) const;

        // Read a bounded binary pack from a filesystem path. I/O failures
        // throw runtime_error; malformed packs throw invalid_argument.
        static resources load(const std::filesystem::path &path);

        // Decode embedded bytes into owned entries; input may then expire.
        // Reject unsupported versions, truncation and trailing bytes.
        static resources decode(std::span<const std::uint8_t> bytes);

        // Encode a deterministic v1 pack. Every name must have exactly one
        // unqualified default. Invalid/incomplete packs throw invalid_argument.
        std::vector<std::uint8_t> encode() const;

        // Encode then write a file (extension is not significant). Existing
        // files are overwritten; publication is not atomic.
        void save(const std::filesystem::path &path) const;

        // Compile a strict v1 JSON manifest. Asset paths are relative to
        // the manifest. Payload contents are copied without conversion.
        static resources from_manifest(const std::filesystem::path &path);

    private:
        std::vector<resource_variant> _entries;
    };
}
