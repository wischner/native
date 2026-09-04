//
// Declares the shared byte-backed TrueType registry, measurements, and
// rasterization used identically by every backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <native/font.h>
#include <native/geometry.h>

namespace native
{
    class img;
}

namespace native::detail
{
    // Owns a rendered alpha image and its offset from the text origin.
    struct rasterized_text
    {
        std::unique_ptr<img> image;
        point offset;
    };

    // Register copied bytes and return zero when validation fails.
    std::uint32_t register_portable_font(
        std::vector<std::uint8_t> bytes,
        int size,
        std::uint32_t face_index,
        font_spec &description);

    // Determine whether an identifier belongs to the portable registry.
    bool is_portable_font(std::uint32_t id);

    // Determine whether a portable identifier still has a registration.
    bool portable_font_valid(std::uint32_t id);

    // Release a portable registration and report whether the ID was
    // one.
    bool release_portable_font(std::uint32_t id);

    // Return shared metrics for a portable registration.
    font_metrics portable_font_metrics(std::uint32_t id);

    // Measure UTF-8 with the shared portable-font path.
    text_metrics measure_portable_text(
        std::uint32_t id,
        const std::string &text);

    // Rasterize UTF-8 into a straight-alpha image in the supplied
    // color.
    rasterized_text rasterize_portable_text(
        std::uint32_t id,
        const std::string &text,
        rgba color);

    // Return descriptions for every valid face in encoded font data.
    std::vector<font_description> describe_font_data(
        const std::vector<std::uint8_t> &bytes,
        const std::filesystem::path &path);

    // Validate and describe one face while returning its byte offset.
    bool inspect_font_face_data(
        const std::vector<std::uint8_t> &bytes,
        std::uint32_t face_index,
        const std::filesystem::path &path,
        font_description &description,
        std::size_t &face_offset);
}
