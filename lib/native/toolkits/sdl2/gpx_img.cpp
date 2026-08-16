//
// Implements the SDL2 image-graphics backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <stdexcept>
#include <algorithm>

#include <SDL2/SDL.h>
#ifdef HAVE_SDL2_TTF
#include <SDL2/SDL_ttf.h>
#endif

#include <native.h>
#include "gpx_img.h"
#include "globals.h"
#include "../../software_image.h"

namespace native
{

    gpx_img::gpx_img(const img &image)
        : _img(image)
        , _clip(0, 0, image.w(), image.h()) {
        // No display dependency needed for software rendering
    }

    gpx &gpx_img::set_clip(const rect &r) {
        _clip = r;
        return *this;
    }

    rect gpx_img::get_clip() const {
        return _clip;
    }

    gpx &gpx_img::clear(rgba color) {
        detail::clear_image(_img, _clip, color);
        return *this;
    }

    gpx &gpx_img::draw_line(point from, point to) {
        detail::draw_image_line(
            _img, _clip, from, to, _ink, _thickness);
        return *this;
    }

    gpx &gpx_img::draw_rect(rect r, bool filled) {
        detail::draw_image_rect(
            _img, _clip, r, _ink, _thickness, filled);
        return *this;
    }

    gpx &gpx_img::draw_native_text(const std::string &text, point p) {
        if (_font && !_font->valid())
            return *this;
#ifdef HAVE_SDL2_TTF
        auto *binding = linux::sdl2::font_bindings.object_from_handle(
            get_font().id());
        TTF_Font *font = binding ? binding->ttf_font : nullptr;
        if (font) {
            // Render text to surface.
            SDL_Color color = {_ink.r, _ink.g, _ink.b, _ink.a};
            SDL_Surface *surface =
                TTF_RenderUTF8_Blended(font, text.c_str(), color);
            if (!surface)
                return *this;

            // Blit surface to our RGBA buffer with clipping
            rgba *dst_pixels = const_cast<rgba *>(_img.pixels());
            SDL_LockSurface(surface);

            int clip_x1 = _clip.p.x, clip_y1 = _clip.p.y;
            int clip_x2 = _clip.x2(), clip_y2 = _clip.y2();

            for (int y = 0; y < surface->h; ++y) {
                for (int x = 0; x < surface->w; ++x) {
                    int dst_x = p.x + x, dst_y = p.y + y;
                    if (dst_x >= clip_x1 && dst_x < clip_x2 &&
                        dst_y >= clip_y1 && dst_y < clip_y2 &&
                        dst_x >= 0 && dst_x < _img.w() && dst_y >= 0 &&
                        dst_y < _img.h()) {
                        // Get pixel from surface (handle different
                        // formats)
                        Uint8 *src_pixel =
                            static_cast<Uint8 *>(surface->pixels) +
                            y * surface->pitch +
                            x * surface->format->BytesPerPixel;
                        Uint32 pixel_val;
                        if (surface->format->BytesPerPixel == 4)
                            pixel_val = *(Uint32 *)src_pixel;
                        else if (surface->format->BytesPerPixel == 3)
                            pixel_val = src_pixel[0] |
                                        (src_pixel[1] << 8) |
                                        (src_pixel[2] << 16);
                        else
                            continue;

                        // Map pixel to RGBA
                        SDL_Color rgba_color;
                        SDL_GetRGBA(pixel_val,
                                    surface->format,
                                    &rgba_color.r,
                                    &rgba_color.g,
                                    &rgba_color.b,
                                    &rgba_color.a);

                        // Only draw non-transparent pixels
                        if (rgba_color.a > 0) {
                            dst_pixels[dst_y * _img.w() + dst_x] =
                                rgba(rgba_color.r,
                                     rgba_color.g,
                                     rgba_color.b,
                                     rgba_color.a);
                        }
                    }
                }
            }

            SDL_UnlockSurface(surface);
            SDL_FreeSurface(surface);
            return *this;
        }
#endif

        SDL_Surface *target =
            SDL_CreateRGBSurfaceFrom(const_cast<rgba *>(_img.pixels()),
                                     _img.w(),
                                     _img.h(),
                                     32,
                                     _img.w() * 4,
                                     0x000000ff,
                                     0x0000ff00,
                                     0x00ff0000,
                                     0xff000000);
        if (!target)
            return *this;
        SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(target);
        if (renderer) {
            SDL_Rect clip = {_clip.p.x,
                             _clip.p.y,
                             static_cast<int>(_clip.d.w),
                             static_cast<int>(_clip.d.h)};
            SDL_RenderSetClipRect(renderer, &clip);
            SDL_Color color = {_ink.r, _ink.g, _ink.b, _ink.a};
            linux::sdl2::draw_text(renderer, text, p.x, p.y, color);
            SDL_RenderPresent(renderer);
            SDL_DestroyRenderer(renderer);
        }
        SDL_FreeSurface(target);
        return *this;
    }

    gpx &gpx_img::draw_img(const img &src, point dst) {
        detail::copy_image(_img, _clip, src, dst);
        return *this;
    }

} // namespace native
