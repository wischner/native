//
// Implements backend-independent font descriptions, file/memory
// creation, and Unicode-character conversion. Stock resources and
// handle destruction remain backend-specific.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/font.h>

#include <fstream>
#include <stdexcept>
#include <utility>

#include "portable_font.h"

namespace
{
    constexpr std::size_t maximum_font_data_size =
        128U * 1024U * 1024U;

    std::string utf8_from(char32_t value) {
        if (value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff))
            throw std::invalid_argument(
                "font_t: invalid Unicode character");

        std::string result;
        if (value <= 0x7f) {
            result.push_back(static_cast<char>(value));
        } else if (value <= 0x7ff) {
            result.push_back(static_cast<char>(0xc0 | (value >> 6)));
            result.push_back(static_cast<char>(0x80 | (value & 0x3f)));
        } else if (value <= 0xffff) {
            result.push_back(static_cast<char>(0xe0 | (value >> 12)));
            result.push_back(
                static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (value & 0x3f)));
        } else {
            result.push_back(static_cast<char>(0xf0 | (value >> 18)));
            result.push_back(
                static_cast<char>(0x80 | ((value >> 12) & 0x3f)));
            result.push_back(
                static_cast<char>(0x80 | ((value >> 6) & 0x3f)));
            result.push_back(static_cast<char>(0x80 | (value & 0x3f)));
        }
        return result;
    }
} // namespace

namespace native
{
    bool font_t::valid() const {
        if (detail::is_portable_font(_id))
            return detail::portable_font_valid(_id);
        return _id != 0;
    }

    const font_spec &font_t::spec() const {
        return _spec;
    }

    std::uint32_t font_t::id() const {
        return _id;
    }

    font_t font_t::from_file(
        const std::string &path,
        int size,
        std::uint32_t face_index) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
            return {};
        stream.seekg(0, std::ios::end);
        const std::streamoff length = stream.tellg();
        if (length <= 0 ||
            static_cast<std::uintmax_t>(length) >
                maximum_font_data_size) {
            return {};
        }
        stream.seekg(0, std::ios::beg);
        std::vector<std::uint8_t> bytes(
            static_cast<std::size_t>(length));
        stream.read(reinterpret_cast<char *>(bytes.data()), length);
        if (!stream)
            return {};
        font_t result;
        result._id = detail::register_portable_font(
            std::move(bytes), size, face_index, result._spec);
        if (result.valid()) {
            result._spec.source = font_source::file;
            result._spec.resource = path;
        }
        return result;
    }

    font_t font_t::from_memory(
        const std::uint8_t *data,
        std::size_t data_size,
        int size,
        std::uint32_t face_index) {
        font_t result;
        if (!data || data_size == 0 ||
            data_size > maximum_font_data_size)
            return result;
        std::vector<std::uint8_t> bytes(data, data + data_size);
        result._id = detail::register_portable_font(
            std::move(bytes), size, face_index, result._spec);
        return result;
    }

    text_metrics font_t::measure_character(char32_t character) const {
        return measure_text(utf8_from(character));
    }
} // namespace native
