//
// Implements strict UTF-8 validation and safe scalar-boundary movement
// for portable clipboard and editor text.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "text_util.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>

namespace
{
    // Decode one scalar and advance offset, rejecting malformed UTF-8.
    bool decode(const std::string &text,
                std::size_t &offset,
                std::uint32_t &value) {
        if (offset >= text.size())
            return false;

        const auto first =
            static_cast<std::uint8_t>(text[offset++]);
        if (first < 0x80) {
            value = first;
            return true;
        }

        int continuation_count = 0;
        std::uint32_t minimum = 0;
        if ((first & 0xe0) == 0xc0) {
            continuation_count = 1;
            value = first & 0x1f;
            minimum = 0x80;
        } else if ((first & 0xf0) == 0xe0) {
            continuation_count = 2;
            value = first & 0x0f;
            minimum = 0x800;
        } else if ((first & 0xf8) == 0xf0) {
            continuation_count = 3;
            value = first & 0x07;
            minimum = 0x10000;
        } else {
            return false;
        }

        for (int index = 0; index < continuation_count; ++index) {
            if (offset >= text.size())
                return false;
            const auto byte =
                static_cast<std::uint8_t>(text[offset++]);
            if ((byte & 0xc0) != 0x80)
                return false;
            value = (value << 6) | (byte & 0x3f);
        }

        return value >= minimum && value <= 0x10ffff &&
               !(value >= 0xd800 && value <= 0xdfff);
    }
} // namespace

namespace native::detail
{
    bool valid_utf8(const std::string &text) {
        std::size_t offset = 0;
        while (offset < text.size()) {
            std::uint32_t value = 0;
            if (!decode(text, offset, value))
                return false;
        }
        return true;
    }

    std::string latin1_to_utf8(const std::uint8_t *data,
                               std::size_t size) {
        std::string result;
        if (!data && size != 0)
            throw std::invalid_argument(
                "Latin-1 conversion requires source bytes");
        if (size > result.max_size() / 2)
            throw std::length_error(
                "Latin-1 text is too large to convert");
        result.reserve(size * 2);
        for (std::size_t index = 0; index < size; ++index) {
            const std::uint8_t value = data[index];
            if (value < 0x80) {
                result.push_back(static_cast<char>(value));
            } else {
                result.push_back(static_cast<char>(0xc0 | value >> 6));
                result.push_back(static_cast<char>(0x80 |
                                                   (value & 0x3f)));
            }
        }
        return result;
    }

    std::size_t previous_utf8(const std::string &text,
                              std::size_t offset) {
        offset = std::min(offset, text.size());
        if (offset == 0)
            return 0;
        --offset;
        while (offset > 0 &&
               (static_cast<std::uint8_t>(text[offset]) & 0xc0) ==
                   0x80) {
            --offset;
        }
        return offset;
    }

    std::size_t next_utf8(const std::string &text,
                          std::size_t offset) {
        offset = std::min(offset, text.size());
        if (offset == text.size())
            return offset;
        ++offset;
        while (offset < text.size() &&
               (static_cast<std::uint8_t>(text[offset]) & 0xc0) ==
                   0x80) {
            ++offset;
        }
        return offset;
    }
} // namespace native::detail
