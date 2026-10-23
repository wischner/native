//
// Implements the SDL2 text-rendering backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <string>

#include <SDL2/SDL.h>
#ifdef HAVE_SDL2_TTF
#include <SDL2/SDL_ttf.h>
#endif

#include <native.h>

#include "globals.h"

namespace
{
    constexpr int k_fallback_scale = 1;
    constexpr int k_glyph_w = 5;
    constexpr int k_glyph_h = 7;

    struct glyph
    {
        char character;
        std::array<std::uint8_t, k_glyph_h> rows;
    };

    // Five-by-seven fallback glyphs used when SDL_ttf is unavailable.
    constexpr glyph fallback_glyphs[] = {
        {'A', {0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}},
        {'B', {0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e}},
        {'C', {0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e}},
        {'D', {0x1c, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1c}},
        {'E', {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f}},
        {'F', {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10}},
        {'G', {0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0e}},
        {'H', {0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}},
        {'I', {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1f}},
        {'J', {0x1f, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0c}},
        {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
        {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f}},
        {'M', {0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11}},
        {'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}},
        {'O', {0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}},
        {'P', {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10}},
        {'Q', {0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d}},
        {'R', {0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11}},
        {'S', {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e}},
        {'T', {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
        {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e}},
        {'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04}},
        {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a}},
        {'X', {0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11}},
        {'Y', {0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04}},
        {'Z', {0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f}},
        {'0', {0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e}},
        {'1', {0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e}},
        {'2', {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f}},
        {'3', {0x1e, 0x01, 0x01, 0x06, 0x01, 0x01, 0x1e}},
        {'4', {0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02}},
        {'5', {0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e}},
        {'6', {0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e}},
        {'7', {0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
        {'8', {0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e}},
        {'9', {0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e}},
        {' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
        {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c}},
        {',', {0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c, 0x08}},
        {':', {0x00, 0x0c, 0x0c, 0x00, 0x0c, 0x0c, 0x00}},
        {';', {0x00, 0x0c, 0x0c, 0x00, 0x0c, 0x0c, 0x08}},
        {'!', {0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04}},
        {'?', {0x0e, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04}},
        {'-', {0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00}},
        {'+', {0x00, 0x04, 0x04, 0x1f, 0x04, 0x04, 0x00}},
        {'_', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f}},
        {'/', {0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10}},
        {'(', {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02}},
        {')', {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08}},
        {'[', {0x0e, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0e}},
        {']', {0x0e, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0e}},
        {'#', {0x0a, 0x0a, 0x1f, 0x0a, 0x1f, 0x0a, 0x0a}},
        {'\'', {0x04, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00}},
        {'"', {0x0a, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00}},
    };

    // Resolve a character to bitmap rows for fallback rendering.
    bool glyph_rows(char character, std::uint8_t rows[k_glyph_h]) {
        if (character >= 'a' && character <= 'z') {
            character = static_cast<char>(
                std::toupper(static_cast<unsigned char>(character)));
        }

        for (const auto &glyph : fallback_glyphs) {
            if (glyph.character != character)
                continue;
            std::copy(glyph.rows.begin(), glyph.rows.end(), rows);
            return true;
        }

        std::fill(rows, rows + k_glyph_h, 0);
        return false;
    }

    int fallback_text_width(const std::string &text) {
        if (text.empty())
            return 0;
        const int adv = (k_glyph_w + 1) * k_fallback_scale;
        return static_cast<int>(text.size()) * adv - k_fallback_scale;
    }

    // Draw text with the built-in five-by-seven bitmap font.
    void draw_fallback_text(SDL_Renderer *renderer,
                            const std::string &text,
                            int x,
                            int y,
                            SDL_Color color) {
        SDL_SetRenderDrawColor(
            renderer, color.r, color.g, color.b, color.a);

        const int px = k_fallback_scale;
        const int adv = (k_glyph_w + 1) * px;

        for (std::size_t i = 0; i < text.size(); ++i) {
            std::uint8_t rows[k_glyph_h] = {};
            const bool known = glyph_rows(text[i], rows);

            if (!known) {
                for (int gy = 0; gy < k_glyph_h; ++gy) {
                    for (int gx = 0; gx < k_glyph_w; ++gx) {
                        const bool border =
                            gy == 0 || gy == k_glyph_h - 1 || gx == 0 ||
                            gx == k_glyph_w - 1;
                        if (!border)
                            continue;
                        SDL_Rect dot{x + static_cast<int>(i) * adv +
                                         gx * px,
                                     y + gy * px,
                                     px,
                                     px};
                        SDL_RenderFillRect(renderer, &dot);
                    }
                }
                continue;
            }

            for (int gy = 0; gy < k_glyph_h; ++gy) {
                const std::uint8_t row = rows[gy];
                for (int gx = 0; gx < k_glyph_w; ++gx) {
                    if ((row & (1u << (k_glyph_w - 1 - gx))) == 0)
                        continue;

                    SDL_Rect dot{x + static_cast<int>(i) * adv +
                                     gx * px,
                                 y + gy * px,
                                 px,
                                 px};
                    SDL_RenderFillRect(renderer, &dot);
                }
            }
        }
    }
} // namespace

namespace linux::sdl2
{
#ifdef HAVE_SDL2_TTF
    bool draw_ttf_text(SDL_Renderer *renderer,
                       sdl2_font &font,
                       const std::string &text,
                       int x,
                       int y,
                       SDL_Color color,
                       sdl2_gpx *cache) {
        if (text.empty())
            return true;
        float scale_x = 1.0F;
        float scale_y = 1.0F;
        SDL_RenderGetScale(renderer, &scale_x, &scale_y);
        const device_face face = device_font(font, scale_x);
        if (!face.font)
            return false;

        const native::rgba ink(color.r, color.g, color.b, color.a);
        // Renderer-owned, bounded LRU: repeated table cells avoid font
        // rasterization and GPU uploads on selection and scroll frames.
        if (cache) {
            for (auto &entry : cache->text_textures) {
                if (entry.font_id != font.id ||
                    entry.face != face.font || entry.color != ink ||
                    entry.text != text)
                    continue;
                entry.used = ++cache->text_clock;
                const SDL_FRect destination{float(x),
                                            float(y),
                                            entry.width / face.scale,
                                            entry.height / face.scale};
                SDL_RenderCopyF(
                    renderer, entry.texture, nullptr, &destination);
                return true;
            }
        }
        SDL_Surface *surface =
            TTF_RenderUTF8_Blended(face.font, text.c_str(), color);
        if (!surface)
            return false;
        SDL_Texture *texture =
            SDL_CreateTextureFromSurface(renderer, surface);
        if (texture) {
            // Device pixels map one to one onto the scaled output.
            const SDL_FRect destination = {
                static_cast<float>(x),
                static_cast<float>(y),
                surface->w / face.scale,
                surface->h / face.scale};
            SDL_RenderCopyF(renderer, texture, nullptr, &destination);
            const auto bytes = std::size_t(surface->w) * surface->h * 4;
            constexpr std::size_t budget = 4 * 1024 * 1024;
            if (cache)
                ++cache->text_uploads;
            if (cache && bytes <= budget) {
                while (!cache->text_textures.empty() &&
                       (cache->text_textures.size() >= 256 ||
                        cache->text_bytes + bytes > budget)) {
                    const auto oldest = std::min_element(
                        cache->text_textures.begin(),
                        cache->text_textures.end(),
                        [](const auto &a, const auto &b) {
                            return a.used < b.used;
                        });
                    cache->text_bytes -=
                        std::size_t(oldest->width) * oldest->height * 4;
                    SDL_DestroyTexture(oldest->texture);
                    cache->text_textures.erase(oldest);
                }
                cache->text_textures.push_back({font.id,
                                                face.font,
                                                text,
                                                ink,
                                                texture,
                                                surface->w,
                                                surface->h,
                                                ++cache->text_clock});
                cache->text_bytes += bytes;
            } else
                SDL_DestroyTexture(texture);
        }
        SDL_FreeSurface(surface);
        return true;
    }
#endif

    int text_width(const std::string &text) {
#ifdef HAVE_SDL2_TTF
        auto *font_handle =
            linux::sdl2::font_bindings.object_from_handle(
                native::font_t::stock(native::font_role::control).id());
        if (font_handle && font_handle->ttf_font) {
            int width = 0;
            int height = 0;
            if (TTF_SizeUTF8(font_handle->ttf_font,
                             text.c_str(),
                             &width,
                             &height) == 0) {
                return width;
            }
        }
#endif
        return fallback_text_width(text);
    }

    int text_height() {
#ifdef HAVE_SDL2_TTF
        auto *font_handle =
            linux::sdl2::font_bindings.object_from_handle(
                native::font_t::stock(native::font_role::control).id());
        if (font_handle && font_handle->ttf_font)
            return TTF_FontHeight(font_handle->ttf_font);
#endif
        return k_glyph_h * k_fallback_scale;
    }

    void draw_text(SDL_Renderer *renderer,
                   const std::string &text,
                   int x,
                   int y,
                   SDL_Color color) {
#ifdef HAVE_SDL2_TTF
        auto *font_handle =
            linux::sdl2::font_bindings.object_from_handle(
                native::font_t::stock(native::font_role::control).id());
        if (font_handle && font_handle->ttf_font &&
            draw_ttf_text(renderer, *font_handle, text, x, y, color))
            return;
#endif
        draw_fallback_text(renderer, text, x, y, color);
    }
} // namespace linux::sdl2
