#include <cctype>
#include <cstring>
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

    bool glyph_rows(char ch, uint8_t rows[k_glyph_h])
    {
        std::memset(rows, 0, k_glyph_h);

        if (ch >= 'a' && ch <= 'z')
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));

        switch (ch)
        {
        case 'A': { uint8_t t[k_glyph_h] = {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'B': { uint8_t t[k_glyph_h] = {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'C': { uint8_t t[k_glyph_h] = {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'D': { uint8_t t[k_glyph_h] = {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'E': { uint8_t t[k_glyph_h] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'F': { uint8_t t[k_glyph_h] = {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'G': { uint8_t t[k_glyph_h] = {0x0E,0x11,0x10,0x17,0x11,0x11,0x0E}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'H': { uint8_t t[k_glyph_h] = {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'I': { uint8_t t[k_glyph_h] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x1F}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'J': { uint8_t t[k_glyph_h] = {0x1F,0x02,0x02,0x02,0x12,0x12,0x0C}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'K': { uint8_t t[k_glyph_h] = {0x11,0x12,0x14,0x18,0x14,0x12,0x11}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'L': { uint8_t t[k_glyph_h] = {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'M': { uint8_t t[k_glyph_h] = {0x11,0x1B,0x15,0x15,0x11,0x11,0x11}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'N': { uint8_t t[k_glyph_h] = {0x11,0x19,0x15,0x13,0x11,0x11,0x11}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'O': { uint8_t t[k_glyph_h] = {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'P': { uint8_t t[k_glyph_h] = {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'Q': { uint8_t t[k_glyph_h] = {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'R': { uint8_t t[k_glyph_h] = {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'S': { uint8_t t[k_glyph_h] = {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'T': { uint8_t t[k_glyph_h] = {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'U': { uint8_t t[k_glyph_h] = {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'V': { uint8_t t[k_glyph_h] = {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'W': { uint8_t t[k_glyph_h] = {0x11,0x11,0x11,0x15,0x15,0x15,0x0A}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'X': { uint8_t t[k_glyph_h] = {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'Y': { uint8_t t[k_glyph_h] = {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}; std::memcpy(rows,t,k_glyph_h); return true; }
        case 'Z': { uint8_t t[k_glyph_h] = {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}; std::memcpy(rows,t,k_glyph_h); return true; }

        case '0': { uint8_t t[k_glyph_h] = {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E}; std::memcpy(rows,t,k_glyph_h); return true; }
        case '1': { uint8_t t[k_glyph_h] = {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E}; std::memcpy(rows,t,k_glyph_h); return true; }
        case '2': { uint8_t t[k_glyph_h] = {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F}; std::memcpy(rows,t,k_glyph_h); return true; }
        case '3': { uint8_t t[k_glyph_h] = {0x1E,0x01,0x01,0x06,0x01,0x01,0x1E}; std::memcpy(rows,t,k_glyph_h); return true; }
        case '4': { uint8_t t[k_glyph_h] = {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02}; std::memcpy(rows,t,k_glyph_h); return true; }
        case '5': { uint8_t t[k_glyph_h] = {0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E}; std::memcpy(rows,t,k_glyph_h); return true; }
        case '6': { uint8_t t[k_glyph_h] = {0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E}; std::memcpy(rows,t,k_glyph_h); return true; }
        case '7': { uint8_t t[k_glyph_h] = {0x1F,0x01,0x02,0x04,0x08,0x08,0x08}; std::memcpy(rows,t,k_glyph_h); return true; }
        case '8': { uint8_t t[k_glyph_h] = {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E}; std::memcpy(rows,t,k_glyph_h); return true; }
        case '9': { uint8_t t[k_glyph_h] = {0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E}; std::memcpy(rows,t,k_glyph_h); return true; }

        case ' ': return true;
        case '.': { uint8_t t[k_glyph_h] = {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C}; std::memcpy(rows,t,k_glyph_h); return true; }
        case ',': { uint8_t t[k_glyph_h] = {0x00,0x00,0x00,0x00,0x0C,0x0C,0x08}; std::memcpy(rows,t,k_glyph_h); return true; }
        case ':': { uint8_t t[k_glyph_h] = {0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x00}; std::memcpy(rows,t,k_glyph_h); return true; }
        case ';': { uint8_t t[k_glyph_h] = {0x00,0x0C,0x0C,0x00,0x0C,0x0C,0x08}; std::memcpy(rows,t,k_glyph_h); return true; }
        case '!': { uint8_t t[k_glyph_h] = {0x04,0x04,0x04,0x04,0x04,0x00,0x04}; std::memcpy(rows,t,k_glyph_h); return true; }
        case '?': { uint8_t t[k_glyph_h] = {0x0E,0x11,0x01,0x02,0x04,0x00,0x04}; std::memcpy(rows,t,k_glyph_h); return true; }
        case '-': { uint8_t t[k_glyph_h] = {0x00,0x00,0x00,0x1F,0x00,0x00,0x00}; std::memcpy(rows,t,k_glyph_h); return true; }
        case '+': { uint8_t t[k_glyph_h] = {0x00,0x04,0x04,0x1F,0x04,0x04,0x00}; std::memcpy(rows,t,k_glyph_h); return true; }
        case '_': { uint8_t t[k_glyph_h] = {0x00,0x00,0x00,0x00,0x00,0x00,0x1F}; std::memcpy(rows,t,k_glyph_h); return true; }
        case '/': { uint8_t t[k_glyph_h] = {0x01,0x02,0x02,0x04,0x08,0x08,0x10}; std::memcpy(rows,t,k_glyph_h); return true; }
        case '(': { uint8_t t[k_glyph_h] = {0x02,0x04,0x08,0x08,0x08,0x04,0x02}; std::memcpy(rows,t,k_glyph_h); return true; }
        case ')': { uint8_t t[k_glyph_h] = {0x08,0x04,0x02,0x02,0x02,0x04,0x08}; std::memcpy(rows,t,k_glyph_h); return true; }
        case '[': { uint8_t t[k_glyph_h] = {0x0E,0x08,0x08,0x08,0x08,0x08,0x0E}; std::memcpy(rows,t,k_glyph_h); return true; }
        case ']': { uint8_t t[k_glyph_h] = {0x0E,0x02,0x02,0x02,0x02,0x02,0x0E}; std::memcpy(rows,t,k_glyph_h); return true; }
        case '#': { uint8_t t[k_glyph_h] = {0x0A,0x0A,0x1F,0x0A,0x1F,0x0A,0x0A}; std::memcpy(rows,t,k_glyph_h); return true; }
        case '\'': { uint8_t t[k_glyph_h] = {0x04,0x04,0x08,0x00,0x00,0x00,0x00}; std::memcpy(rows,t,k_glyph_h); return true; }
        case '"': { uint8_t t[k_glyph_h] = {0x0A,0x0A,0x00,0x00,0x00,0x00,0x00}; std::memcpy(rows,t,k_glyph_h); return true; }

        default:
            return false;
        }
    }

    int fallback_text_width(const std::string &text)
    {
        if (text.empty())
            return 0;
        const int adv = (k_glyph_w + 1) * k_fallback_scale;
        return static_cast<int>(text.size()) * adv - k_fallback_scale;
    }

    void draw_fallback_text(SDL_Renderer *r, const std::string &text, int x, int y, SDL_Color col)
    {
        SDL_SetRenderDrawColor(r, col.r, col.g, col.b, col.a);

        const int px = k_fallback_scale;
        const int adv = (k_glyph_w + 1) * px;

        for (std::size_t i = 0; i < text.size(); ++i)
        {
            uint8_t rows[k_glyph_h] = {};
            bool known = glyph_rows(text[i], rows);

            if (!known)
            {
                for (int gy = 0; gy < k_glyph_h; ++gy)
                {
                    for (int gx = 0; gx < k_glyph_w; ++gx)
                    {
                        const bool border = (gy == 0 || gy == k_glyph_h - 1 || gx == 0 || gx == k_glyph_w - 1);
                        if (!border)
                            continue;
                        SDL_Rect dot{
                            x + static_cast<int>(i) * adv + gx * px,
                            y + gy * px,
                            px,
                            px};
                        SDL_RenderFillRect(r, &dot);
                    }
                }
                continue;
            }

            for (int gy = 0; gy < k_glyph_h; ++gy)
            {
                const uint8_t row = rows[gy];
                for (int gx = 0; gx < k_glyph_w; ++gx)
                {
                    if ((row & (1u << (k_glyph_w - 1 - gx))) == 0)
                        continue;

                    SDL_Rect dot{
                        x + static_cast<int>(i) * adv + gx * px,
                        y + gy * px,
                        px,
                        px};
                    SDL_RenderFillRect(r, &dot);
                }
            }
        }
    }
}

namespace sdl
{
    int text_width(const std::string &text)
    {
#ifdef HAVE_SDL2_TTF
        auto *fh = sdl::font_bindings.from_a(native::font_t::stock(native::font_role::control).id());
        if (fh && fh->ttf_font)
        {
            int w = 0;
            int h = 0;
            if (TTF_SizeUTF8(fh->ttf_font, text.c_str(), &w, &h) == 0)
                return w;
        }
#endif
        return fallback_text_width(text);
    }

    int text_height()
    {
#ifdef HAVE_SDL2_TTF
        auto *fh = sdl::font_bindings.from_a(native::font_t::stock(native::font_role::control).id());
        if (fh && fh->ttf_font)
            return TTF_FontHeight(fh->ttf_font);
#endif
        return k_glyph_h * k_fallback_scale;
    }

    void draw_text(SDL_Renderer *r, const std::string &text, int x, int y, SDL_Color col)
    {
#ifdef HAVE_SDL2_TTF
        auto *fh = sdl::font_bindings.from_a(native::font_t::stock(native::font_role::control).id());
        if (fh && fh->ttf_font)
        {
            SDL_Surface *surf = TTF_RenderUTF8_Solid(fh->ttf_font, text.c_str(), col);
            if (surf)
            {
                SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
                if (tex)
                {
                    SDL_Rect dst = {x, y, surf->w, surf->h};
                    SDL_RenderCopy(r, tex, nullptr, &dst);
                    SDL_DestroyTexture(tex);
                }
                SDL_FreeSurface(surf);
                return;
            }
        }
#endif
        draw_fallback_text(r, text, x, y, col);
    }
} // namespace sdl
