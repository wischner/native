//
// Implements the shared byte-backed TrueType registry, UTF-8 layout,
// measurements, and alpha rasterization used by every backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "portable_font.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <unordered_map>

#include <native/graphics.h>

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include "third_party/stb_truetype.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace
{
    constexpr std::uint32_t portable_id_base = 0xf0000000U;

    struct portable_face
    {
        std::vector<std::uint8_t> bytes;
        stbtt_fontinfo info{};
        native::font_description description;
        native::font_metrics metrics;
        float scale = 0.0f;
        int pixel_size = 0;
        std::size_t face_offset = 0;
    };

    struct registry_state
    {
        std::mutex mutex;
        std::unordered_map<
            std::uint32_t,
            std::unique_ptr<portable_face>> faces;
        std::uint32_t next_id = portable_id_base;
    };

    registry_state &registry() {
        static auto *value = new registry_state();
        return *value;
    }
    bool initialize_face(
        portable_face &face,
        std::uint32_t face_index) {
        if (!native::detail::inspect_font_face_data(
                face.bytes,
                face_index,
                "",
                face.description,
                face.face_offset) ||
            face.face_offset >
                static_cast<std::size_t>(
                    std::numeric_limits<int>::max())) {
            return false;
        }
        return stbtt_InitFont(
                   &face.info,
                   face.bytes.data(),
                   static_cast<int>(face.face_offset)) != 0;
    }

    std::vector<int> decode_utf8(const std::string &text) {
        std::vector<int> result;
        std::size_t index = 0;
        while (index < text.size()) {
            const auto lead =
                static_cast<unsigned char>(text[index]);
            std::uint32_t value = 0xfffdU;
            std::size_t count = 1;
            if (lead < 0x80U) {
                value = lead;
            } else if (lead >= 0xc2U && lead <= 0xdfU) {
                count = 2;
                value = lead & 0x1fU;
            } else if (lead >= 0xe0U && lead <= 0xefU) {
                count = 3;
                value = lead & 0x0fU;
            } else if (lead >= 0xf0U && lead <= 0xf4U) {
                count = 4;
                value = lead & 0x07U;
            }

            bool valid = count > 1;
            if (count == 1 && lead < 0x80U)
                valid = true;
            if (index + count > text.size())
                valid = false;
            for (std::size_t part = 1; valid && part < count; ++part) {
                const auto next = static_cast<unsigned char>(
                    text[index + part]);
                if ((next & 0xc0U) != 0x80U) {
                    valid = false;
                } else {
                    value = (value << 6U) | (next & 0x3fU);
                }
            }
            if ((count == 2 && value < 0x80U) ||
                (count == 3 && value < 0x800U) ||
                (count == 4 && value < 0x10000U) ||
                value > 0x10ffffU ||
                (value >= 0xd800U && value <= 0xdfffU)) {
                valid = false;
            }
            if (valid) {
                result.push_back(static_cast<int>(value));
                index += count;
            } else {
                result.push_back(0xfffd);
                ++index;
            }
        }
        return result;
    }

    native::font_metrics calculate_metrics(const portable_face &face) {
        int ascent = 0;
        int descent = 0;
        int leading = 0;
        stbtt_GetFontVMetrics(
            &face.info, &ascent, &descent, &leading);
        int maximum_advance = 0;
        for (int glyph = 0; glyph < face.info.numGlyphs; ++glyph) {
            int advance = 0;
            int bearing = 0;
            stbtt_GetGlyphHMetrics(
                &face.info, glyph, &advance, &bearing);
            maximum_advance = std::max(maximum_advance, advance);
        }
        const int scaled_ascent = std::max(
            1, static_cast<int>(std::ceil(ascent * face.scale)));
        const int scaled_descent = std::max(
            1, static_cast<int>(std::ceil(-descent * face.scale)));
        const int scaled_leading = std::max(
            1, static_cast<int>(std::ceil(leading * face.scale)));
        return {
            scaled_ascent,
            scaled_descent,
            scaled_leading,
            scaled_ascent + scaled_descent + scaled_leading,
            std::max(
                1,
                static_cast<int>(
                    std::ceil(maximum_advance * face.scale)))};
    }

    struct text_layout
    {
        int left = 0;
        int top = 0;
        int right = 0;
        int bottom = 0;
        int advance = 0;
        bool visible = false;
    };

    text_layout layout(
        const portable_face &face,
        const std::vector<int> &codepoints) {
        text_layout result;
        const native::font_metrics &face_metrics = face.metrics;
        result.top = 0;
        result.bottom = face_metrics.height;
        float pen = 0.0f;
        for (std::size_t index = 0;
             index < codepoints.size();
             ++index) {
            int x0 = 0;
            int y0 = 0;
            int x1 = 0;
            int y1 = 0;
            stbtt_GetCodepointBitmapBox(
                &face.info,
                codepoints[index],
                face.scale,
                face.scale,
                &x0,
                &y0,
                &x1,
                &y1);
            if (x1 > x0 && y1 > y0) {
                const int glyph_left =
                    static_cast<int>(std::lround(pen)) + x0;
                const int glyph_top = face_metrics.ascent + y0;
                if (!result.visible) {
                    result.left = glyph_left;
                    result.right =
                        static_cast<int>(std::lround(pen)) + x1;
                    result.top = std::min(0, glyph_top);
                    result.bottom =
                        std::max(face_metrics.height,
                                 face_metrics.ascent + y1);
                    result.visible = true;
                } else {
                    result.left = std::min(result.left, glyph_left);
                    result.right = std::max(
                        result.right,
                        static_cast<int>(std::lround(pen)) + x1);
                    result.top = std::min(result.top, glyph_top);
                    result.bottom = std::max(
                        result.bottom, face_metrics.ascent + y1);
                }
            }

            int advance = 0;
            int bearing = 0;
            stbtt_GetCodepointHMetrics(
                &face.info,
                codepoints[index],
                &advance,
                &bearing);
            pen += advance * face.scale;
            if (index + 1 < codepoints.size()) {
                pen += stbtt_GetCodepointKernAdvance(
                           &face.info,
                           codepoints[index],
                           codepoints[index + 1]) *
                    face.scale;
            }
        }
        result.advance = static_cast<int>(std::lround(pen));
        return result;
    }
} // namespace

namespace native::detail
{
    std::uint32_t register_portable_font(
        std::vector<std::uint8_t> bytes,
        int size,
        std::uint32_t face_index,
        font_spec &description) {
        if (bytes.empty() || size <= 0 || size > 4096)
            return 0;
        auto face = std::make_unique<portable_face>();
        face->bytes = std::move(bytes);
        face->pixel_size = size;
        if (!initialize_face(*face, face_index))
            return 0;
        face->scale = stbtt_ScaleForPixelHeight(
            &face->info, static_cast<float>(size));
        if (!(face->scale > 0.0f))
            return 0;
        face->metrics = calculate_metrics(*face);
        description.family = face->description.family;
        description.style = face->description.style;
        description.size = size;
        description.weight = face->description.weight;
        description.italic = face->description.italic;
        description.face_index = face_index;
        description.source = font_source::memory;

        registry_state &state = registry();
        std::lock_guard<std::mutex> lock(state.mutex);
        do {
            ++state.next_id;
            if (state.next_id < portable_id_base)
                state.next_id = portable_id_base + 1U;
        } while (state.faces.contains(state.next_id));
        const std::uint32_t id = state.next_id;
        state.faces.emplace(id, std::move(face));
        return id;
    }

    bool is_portable_font(std::uint32_t id) {
        return id > portable_id_base;
    }

    bool portable_font_valid(std::uint32_t id) {
        if (!is_portable_font(id))
            return false;
        registry_state &state = registry();
        std::lock_guard<std::mutex> lock(state.mutex);
        return state.faces.contains(id);
    }

    bool release_portable_font(std::uint32_t id) {
        if (!is_portable_font(id))
            return false;
        registry_state &state = registry();
        std::lock_guard<std::mutex> lock(state.mutex);
        state.faces.erase(id);
        return true;
    }

    font_metrics portable_font_metrics(std::uint32_t id) {
        registry_state &state = registry();
        std::lock_guard<std::mutex> lock(state.mutex);
        const auto found = state.faces.find(id);
        return found == state.faces.end() ? font_metrics()
                                          : found->second->metrics;
    }

    text_metrics measure_portable_text(
        std::uint32_t id,
        const std::string &text) {
        registry_state &state = registry();
        std::lock_guard<std::mutex> lock(state.mutex);
        const auto found = state.faces.find(id);
        if (found == state.faces.end())
            return {};
        const portable_face &face = *found->second;
        const font_metrics &face_metrics = face.metrics;
        const text_layout text_layout = layout(face, decode_utf8(text));
        return {
            text_layout.visible
                ? std::max(0, text_layout.right - text_layout.left)
                : 0,
            face_metrics.height,
            text_layout.advance};
    }

    rasterized_text rasterize_portable_text(
        std::uint32_t id,
        const std::string &text,
        rgba color) {
        rasterized_text result;
        registry_state &state = registry();
        std::lock_guard<std::mutex> lock(state.mutex);
        const auto found = state.faces.find(id);
        if (found == state.faces.end())
            return result;
        const portable_face &face = *found->second;
        const std::vector<int> codepoints = decode_utf8(text);
        const text_layout text_layout = layout(face, codepoints);
        if (!text_layout.visible)
            return result;
        const int width = text_layout.right - text_layout.left;
        const int height = text_layout.bottom - text_layout.top;
        if (width <= 0 || height <= 0 ||
            width > std::numeric_limits<dim>::max() ||
            height > std::numeric_limits<dim>::max()) {
            return result;
        }
        result.image = std::make_unique<img>(
            static_cast<dim>(width), static_cast<dim>(height));
        result.offset = point(
            static_cast<coord>(text_layout.left),
            static_cast<coord>(text_layout.top));
        rgba *pixels = result.image->pixels();
        const font_metrics &face_metrics = face.metrics;
        float pen = 0.0f;
        for (std::size_t index = 0;
             index < codepoints.size();
             ++index) {
            int x0 = 0;
            int y0 = 0;
            int x1 = 0;
            int y1 = 0;
            stbtt_GetCodepointBitmapBox(
                &face.info,
                codepoints[index],
                face.scale,
                face.scale,
                &x0,
                &y0,
                &x1,
                &y1);
            const int glyph_width = x1 - x0;
            const int glyph_height = y1 - y0;
            if (glyph_width > 0 && glyph_height > 0) {
                std::vector<std::uint8_t> coverage(
                    static_cast<std::size_t>(glyph_width) *
                    glyph_height);
                stbtt_MakeCodepointBitmap(
                    &face.info,
                    coverage.data(),
                    glyph_width,
                    glyph_height,
                    glyph_width,
                    face.scale,
                    face.scale,
                    codepoints[index]);
                const int destination_x =
                    static_cast<int>(std::lround(pen)) + x0 -
                    text_layout.left;
                const int destination_y =
                    face_metrics.ascent + y0 - text_layout.top;
                for (int y = 0; y < glyph_height; ++y) {
                    for (int x = 0; x < glyph_width; ++x) {
                        const std::uint8_t mask =
                            coverage[static_cast<std::size_t>(y) *
                                         glyph_width +
                                     x];
                        if (mask == 0)
                            continue;
                        rgba &destination = pixels[
                            (destination_y + y) * width +
                            destination_x + x];
                        const unsigned alpha =
                            (static_cast<unsigned>(color.a) * mask +
                             127U) /
                            255U;
                        const unsigned combined =
                            alpha +
                            (destination.a * (255U - alpha) + 127U) /
                                255U;
                        destination = rgba(
                            color.r,
                            color.g,
                            color.b,
                            static_cast<std::uint8_t>(combined));
                    }
                }
            }

            int advance = 0;
            int bearing = 0;
            stbtt_GetCodepointHMetrics(
                &face.info,
                codepoints[index],
                &advance,
                &bearing);
            pen += advance * face.scale;
            if (index + 1 < codepoints.size()) {
                pen += stbtt_GetCodepointKernAdvance(
                           &face.info,
                           codepoints[index],
                           codepoints[index + 1]) *
                    face.scale;
            }
        }
        return result;
    }

} // namespace native::detail
