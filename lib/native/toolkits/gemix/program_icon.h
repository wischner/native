//
// Declares the GEMix desktop program icon, an icon-only GEM resource
// found as icons/<program>.rsc on the GEM resource search path.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#pragma once

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

#include <native/graphics.h>

namespace native::detail
{
    //
    // Encode a 32x32 monochrome icon-only GEM resource.
    //
    // Parameters:
    //      icon        - Source image; it is fitted into 32x32 pixels.
    //      label       - Icon label text, stored after the bitmaps.
    //
    // Returns:
    //      Resource bytes in the GEMix host layout: one ICONBLK, one free
    //      string, then the mask and data planes.
    //
    // Notes:
    //      Pixels follow the SDK resgen rule: alpha below 32 is
    //      transparent, and a visible pixel with any channel below 224
    //      is black.
    //
    std::vector<std::uint8_t> encode_gem_program_icon(
        const img &icon, std::string_view label);

    //
    // Install a program icon for the running executable when missing.
    //
    // Parameters:
    //      icon        - Main-window icon image.
    //
    // Notes:
    //      Writes icons/<executable stem>.rsc under $GEM_RESOURCE_DIR,
    //      or under $XDG_DATA_HOME/gem (default ~/.local/share/gem).
    //      An existing file is never replaced. Every failure is ignored
    //      so that the application continues normally.
    //
    void publish_gem_program_icon(const img &icon) noexcept;

    //
    // Return the directory receiving program icons, or an empty path.
    //
    std::filesystem::path gem_program_icon_directory();
} // namespace native::detail
