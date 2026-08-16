//
// Declares the private portable clipboard payload exchanged with each
// operating-system or toolkit backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>
#include <vector>

#include <cstdint>

namespace native::detail
{
    // Holds copied UTF-8 text and encoded PNG clipboard data.
    struct clipboard_payload
    {
        bool has_text = false;
        bool has_image = false;
        std::string text;
        std::vector<std::uint8_t> image;
    };

    // Copy the current native clipboard into portable ownership.
    clipboard_payload read_clipboard();

    // Publish a complete portable payload to the native clipboard.
    void write_clipboard(const clipboard_payload &payload);
} // namespace native::detail
