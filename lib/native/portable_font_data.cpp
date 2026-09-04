//
// Validates SFNT font tables and extracts portable face metadata
// without constructing backend or rasterizer objects.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "portable_font.h"

#include <algorithm>
#include <utility>

namespace
{
    constexpr std::uint32_t tag(char a, char b, char c, char d) {
        return (static_cast<std::uint32_t>(a) << 24U) |
            (static_cast<std::uint32_t>(b) << 16U) |
            (static_cast<std::uint32_t>(c) << 8U) |
            static_cast<std::uint32_t>(d);
    }

    struct table_view
    {
        std::size_t offset = 0;
        std::size_t size = 0;
    };

    bool read_u16(const std::vector<std::uint8_t> &bytes,
                  std::size_t offset,
                  std::uint16_t &value) {
        if (offset > bytes.size() || bytes.size() - offset < 2)
            return false;
        value = static_cast<std::uint16_t>(
            (static_cast<unsigned>(bytes[offset]) << 8U) |
            bytes[offset + 1]);
        return true;
    }

    bool read_u32(const std::vector<std::uint8_t> &bytes,
                  std::size_t offset,
                  std::uint32_t &value) {
        if (offset > bytes.size() || bytes.size() - offset < 4)
            return false;
        value = (static_cast<std::uint32_t>(bytes[offset]) << 24U) |
            (static_cast<std::uint32_t>(bytes[offset + 1]) << 16U) |
            (static_cast<std::uint32_t>(bytes[offset + 2]) << 8U) |
            static_cast<std::uint32_t>(bytes[offset + 3]);
        return true;
    }

    bool find_face_offset(
        const std::vector<std::uint8_t> &bytes,
        std::uint32_t face_index,
        std::size_t &offset) {
        std::uint32_t signature = 0;
        if (!read_u32(bytes, 0, signature))
            return false;
        if (signature != tag('t', 't', 'c', 'f')) {
            if (face_index != 0)
                return false;
            offset = 0;
            return true;
        }

        std::uint32_t count = 0;
        if (!read_u32(bytes, 8, count) || face_index >= count ||
            count > 1024U) {
            return false;
        }
        std::uint32_t value = 0;
        if (!read_u32(bytes, 12U + face_index * 4U, value))
            return false;
        offset = value;
        return offset < bytes.size();
    }

    bool table_at(const std::vector<std::uint8_t> &bytes,
                  std::size_t font_offset,
                  std::uint32_t wanted_tag,
                  table_view &result) {
        std::uint16_t count = 0;
        if (!read_u16(bytes, font_offset + 4, count) || count > 256U)
            return false;
        const std::size_t directory = font_offset + 12;
        if (directory > bytes.size() ||
            static_cast<std::size_t>(count) >
                (bytes.size() - directory) / 16U) {
            return false;
        }
        for (std::uint16_t index = 0; index < count; ++index) {
            const std::size_t record = directory + index * 16U;
            std::uint32_t record_tag = 0;
            std::uint32_t offset = 0;
            std::uint32_t size = 0;
            if (!read_u32(bytes, record, record_tag) ||
                !read_u32(bytes, record + 8, offset) ||
                !read_u32(bytes, record + 12, size)) {
                return false;
            }
            if (offset > bytes.size() || size > bytes.size() - offset)
                return false;
            if (record_tag == wanted_tag)
                result = {offset, size};
        }
        return result.size != 0;
    }

    bool valid_face(const std::vector<std::uint8_t> &bytes,
                    std::size_t offset) {
        std::uint32_t signature = 0;
        if (!read_u32(bytes, offset, signature))
            return false;
        const bool valid_signature = signature == 0x00010000U ||
            signature == tag('t', 'r', 'u', 'e') ||
            signature == tag('t', 'y', 'p', '1') ||
            signature == tag('O', 'T', 'T', 'O');
        if (!valid_signature)
            return false;

        table_view cmap;
        table_view head;
        table_view hhea;
        table_view hmtx;
        table_view maxp;
        table_view glyf;
        table_view loca;
        table_view cff;
        const bool common =
            table_at(bytes, offset, tag('c', 'm', 'a', 'p'), cmap) &&
            cmap.size >= 4 &&
            table_at(bytes, offset, tag('h', 'e', 'a', 'd'), head) &&
            head.size >= 54 &&
            table_at(bytes, offset, tag('h', 'h', 'e', 'a'), hhea) &&
            hhea.size >= 36 &&
            table_at(bytes, offset, tag('h', 'm', 't', 'x'), hmtx) &&
            table_at(bytes, offset, tag('m', 'a', 'x', 'p'), maxp) &&
            maxp.size >= 6;
        const bool outlines =
            (table_at(bytes, offset, tag('g', 'l', 'y', 'f'), glyf) &&
             table_at(bytes, offset, tag('l', 'o', 'c', 'a'), loca)) ||
            table_at(bytes, offset, tag('C', 'F', 'F', ' '), cff);
        return common && outlines;
    }

    void append_utf8(std::string &text, std::uint32_t value) {
        if (value <= 0x7fU) {
            text.push_back(static_cast<char>(value));
        } else if (value <= 0x7ffU) {
            text.push_back(static_cast<char>(0xc0U | (value >> 6U)));
            text.push_back(
                static_cast<char>(0x80U | (value & 0x3fU)));
        } else if (value <= 0xffffU) {
            text.push_back(static_cast<char>(0xe0U | (value >> 12U)));
            text.push_back(static_cast<char>(
                0x80U | ((value >> 6U) & 0x3fU)));
            text.push_back(
                static_cast<char>(0x80U | (value & 0x3fU)));
        } else {
            text.push_back(static_cast<char>(0xf0U | (value >> 18U)));
            text.push_back(static_cast<char>(
                0x80U | ((value >> 12U) & 0x3fU)));
            text.push_back(static_cast<char>(
                0x80U | ((value >> 6U) & 0x3fU)));
            text.push_back(
                static_cast<char>(0x80U | (value & 0x3fU)));
        }
    }

    std::string utf16_name(
        const std::uint8_t *data, std::size_t length) {
        std::string result;
        for (std::size_t index = 0; index + 1 < length; index += 2) {
            std::uint32_t value =
                (static_cast<unsigned>(data[index]) << 8U) |
                data[index + 1];
            if (value >= 0xd800U && value <= 0xdbffU &&
                index + 3 < length) {
                const std::uint32_t low =
                    (static_cast<unsigned>(data[index + 2]) << 8U) |
                    data[index + 3];
                if (low >= 0xdc00U && low <= 0xdfffU) {
                    value = 0x10000U +
                        ((value - 0xd800U) << 10U) +
                        (low - 0xdc00U);
                    index += 2;
                }
            }
            if (value >= 0xd800U && value <= 0xdfffU)
                value = 0xfffdU;
            append_utf8(result, value);
        }
        return result;
    }

    std::string byte_name(
        const std::uint8_t *data, std::size_t length) {
        std::string result;
        for (std::size_t index = 0; index < length; ++index)
            append_utf8(result, data[index]);
        return result;
    }

    std::string font_name(const std::vector<std::uint8_t> &bytes,
                          const table_view &name,
                          std::uint16_t wanted_name) {
        std::uint16_t count = 0;
        std::uint16_t strings = 0;
        if (name.size < 6 || !read_u16(bytes, name.offset + 2, count) ||
            !read_u16(bytes, name.offset + 4, strings) ||
            count > (name.size - 6U) / 12U || strings > name.size) {
            return {};
        }

        std::string best;
        int best_score = -1;
        for (std::uint16_t index = 0; index < count; ++index) {
            const std::size_t record = name.offset + 6U + index * 12U;
            std::uint16_t platform = 0;
            std::uint16_t language = 0;
            std::uint16_t name_id = 0;
            std::uint16_t length = 0;
            std::uint16_t offset = 0;
            if (!read_u16(bytes, record, platform) ||
                !read_u16(bytes, record + 4, language) ||
                !read_u16(bytes, record + 6, name_id) ||
                !read_u16(bytes, record + 8, length) ||
                !read_u16(bytes, record + 10, offset) ||
                name_id != wanted_name) {
                continue;
            }
            const std::size_t relative =
                static_cast<std::size_t>(strings) + offset;
            if (relative > name.size || length > name.size - relative)
                continue;

            int score = 0;
            if (platform == 3 && language == 0x0409U)
                score = 40;
            else if (platform == 0)
                score = 30;
            else if (platform == 3)
                score = 20;
            else if (platform == 1)
                score = 10;
            if (score <= best_score)
                continue;

            const std::uint8_t *value =
                bytes.data() + name.offset + relative;
            std::string decoded = platform == 0 || platform == 3
                ? utf16_name(value, length)
                : byte_name(value, length);
            if (!decoded.empty()) {
                best = std::move(decoded);
                best_score = score;
            }
        }
        return best;
    }

    native::font_description describe(
        const std::vector<std::uint8_t> &bytes,
        std::size_t face_offset,
        const std::filesystem::path &path,
        std::uint32_t face_index) {
        native::font_description result;
        result.path = path;
        result.face_index = face_index;

        table_view names;
        if (table_at(
                bytes,
                face_offset,
                tag('n', 'a', 'm', 'e'),
                names)) {
            result.family = font_name(bytes, names, 1);
            result.style = font_name(bytes, names, 2);
            result.face_name = font_name(bytes, names, 4);
        }

        table_view os2;
        std::uint16_t weight = 400;
        if (table_at(bytes,
                     face_offset,
                     tag('O', 'S', '/', '2'),
                     os2) &&
            os2.size >= 6) {
            read_u16(bytes, os2.offset + 4, weight);
        }
        result.weight = weight;

        table_view head;
        std::uint16_t mac_style = 0;
        if (table_at(bytes,
                     face_offset,
                     tag('h', 'e', 'a', 'd'),
                     head) &&
            head.size >= 46) {
            read_u16(bytes, head.offset + 44, mac_style);
        }
        result.italic = (mac_style & 2U) != 0 ||
            result.style.find("Italic") != std::string::npos ||
            result.style.find("Oblique") != std::string::npos;

        table_view post;
        std::uint32_t fixed_pitch = 0;
        if (table_at(bytes,
                     face_offset,
                     tag('p', 'o', 's', 't'),
                     post) &&
            post.size >= 16) {
            read_u32(bytes, post.offset + 12, fixed_pitch);
        }
        result.fixed_pitch = fixed_pitch != 0;
        return result;
    }
} // namespace

namespace native::detail
{
    bool inspect_font_face_data(
        const std::vector<std::uint8_t> &bytes,
        std::uint32_t face_index,
        const std::filesystem::path &path,
        font_description &description,
        std::size_t &face_offset) {
        if (!find_face_offset(bytes, face_index, face_offset) ||
            !valid_face(bytes, face_offset)) {
            return false;
        }
        description = describe(bytes, face_offset, path, face_index);
        return true;
    }

    std::vector<font_description> describe_font_data(
        const std::vector<std::uint8_t> &bytes,
        const std::filesystem::path &path) {
        std::vector<font_description> result;
        std::uint32_t signature = 0;
        if (!read_u32(bytes, 0, signature))
            return result;
        std::uint32_t count = 1;
        if (signature == tag('t', 't', 'c', 'f') &&
            !read_u32(bytes, 8, count)) {
            return result;
        }
        count = std::min(count, 1024U);
        for (std::uint32_t index = 0; index < count; ++index) {
            font_description description;
            std::size_t face_offset = 0;
            if (inspect_font_face_data(bytes,
                                       index,
                                       path,
                                       description,
                                       face_offset)) {
                result.push_back(std::move(description));
            }
        }
        return result;
    }
} // namespace native::detail
