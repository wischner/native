#include <stdexcept>
#include <cmath>

#include <SDL2/SDL.h>
#ifdef HAVE_SDL2_TTF
#include <SDL2/SDL_ttf.h>
#endif

#include <native.h>
#include "gpx_wnd.h"
#include "globals.h"

static void apply_sdl_state(SDL_Renderer *renderer, native::gpx_wnd *self, sdl::sdl2gpx *cache)
{
    if (!cache)
        return;

    // Set draw color if changed
    if (cache->current_fg != self->ink())
    {
        native::rgba c = self->ink();
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
        cache->current_fg = self->ink();
    }

    // Set clip rectangle
    SDL_Rect clip_rect = {
        self->clip().p.x,
        self->clip().p.y,
        static_cast<int>(self->clip().d.w),
        static_cast<int>(self->clip().d.h)};
    SDL_RenderSetClipRect(renderer, &clip_rect);
}

namespace native
{

    gpx_wnd::gpx_wnd(const wnd *window, point offset)
        : _wnd(const_cast<wnd *>(window)), _offset(offset)
    {
        SDL_Window *win = sdl::wnd_bindings.from_b(_wnd);
        if (!win)
            throw std::runtime_error("SDL2: No window available for gpx_wnd");

        // Get or create renderer
        auto *cache = sdl::wnd_gpx_bindings.from_a(_wnd);
        if (!cache)
        {
            cache = new sdl::sdl2gpx();
            cache->renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
            if (!cache->renderer)
                cache->renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
            sdl::wnd_gpx_bindings.register_pair(_wnd, cache);
        }
    }

    gpx_wnd::~gpx_wnd() = default;

    gpx &gpx_wnd::set_clip(const rect &r)
    {
        _clip = r;
        return *this;
    }

    rect gpx_wnd::clip() const
    {
        return _clip;
    }

    gpx &gpx_wnd::clear(rgba color)
    {
        auto *cache = sdl::wnd_gpx_bindings.from_a(_wnd);
        if (!cache || !cache->renderer)
            return *this;

        SDL_Renderer *renderer = cache->renderer;

        // Set clip region
        SDL_Rect clip_rect = {_clip.p.x, _clip.p.y, static_cast<int>(_clip.d.w), static_cast<int>(_clip.d.h)};
        SDL_RenderSetClipRect(renderer, &clip_rect);

        // Clear with color
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_RenderFillRect(renderer, &clip_rect);
        cache->current_fg = color;
        return *this;
    }

    gpx &gpx_wnd::draw_line(point from, point to)
    {
        auto *cache = sdl::wnd_gpx_bindings.from_a(_wnd);
        if (!cache || !cache->renderer)
            return *this;

        SDL_Renderer *renderer = cache->renderer;
        apply_sdl_state(renderer, this, cache);

        SDL_RenderDrawLine(renderer, from.x, from.y, to.x, to.y);
        return *this;
    }

    gpx &gpx_wnd::draw_rect(rect r, bool filled)
    {
        auto *cache = sdl::wnd_gpx_bindings.from_a(_wnd);
        if (!cache || !cache->renderer)
            return *this;

        SDL_Renderer *renderer = cache->renderer;
        apply_sdl_state(renderer, this, cache);

        SDL_Rect sdl_rect = {r.p.x, r.p.y, static_cast<int>(r.d.w), static_cast<int>(r.d.h)};

        if (filled)
            SDL_RenderFillRect(renderer, &sdl_rect);
        else
            SDL_RenderDrawRect(renderer, &sdl_rect);
        return *this;
    }

    gpx &gpx_wnd::draw_text(const std::string &text, point p)
    {
        auto *cache = sdl::wnd_gpx_bindings.from_a(_wnd);
        if (!cache || !cache->renderer)
            return *this;

        SDL_Renderer *renderer = cache->renderer;
        apply_sdl_state(renderer, this, cache);

#ifdef HAVE_SDL2_TTF
        if (!font().valid())
        {
            SDL_Color color = {ink().r, ink().g, ink().b, ink().a};
            sdl::draw_text(renderer, text, p.x, p.y, color);
            return *this;
        }

        auto *fh = sdl::font_bindings.from_a(font().id());
        if (fh && fh->ttf_font)
        {
            SDL_Color color = {ink().r, ink().g, ink().b, ink().a};
            SDL_Surface *surface = TTF_RenderText_Solid(fh->ttf_font, text.c_str(), color);
            if (surface)
            {
                SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
                if (texture)
                {
                    SDL_Rect dst_rect = {p.x, p.y, surface->w, surface->h};
                    SDL_RenderCopy(renderer, texture, nullptr, &dst_rect);
                    SDL_DestroyTexture(texture);
                }
                SDL_FreeSurface(surface);
                return *this;
            }
        }
#endif
        SDL_Color fallback = {ink().r, ink().g, ink().b, ink().a};
        sdl::draw_text(renderer, text, p.x, p.y, fallback);
        return *this;
    }

    gpx &gpx_wnd::draw_img(const img &src, point dst)
    {
        auto *cache = sdl::wnd_gpx_bindings.from_a(_wnd);
        if (!cache || !cache->renderer)
            return *this;

        SDL_Renderer *renderer = cache->renderer;
        apply_sdl_state(renderer, this, cache);

        // Create surface from RGBA pixel data
        SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(
            const_cast<rgba *>(src.pixels()),
            src.w(), src.h(), 32, src.w() * 4,
            0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000);

        if (!surface)
            return *this;

        // Create texture and render
        SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
        if (texture)
        {
            SDL_Rect dst_rect = {dst.x, dst.y, src.w(), src.h()};
            SDL_RenderCopy(renderer, texture, nullptr, &dst_rect);
            SDL_DestroyTexture(texture);
        }

        SDL_FreeSurface(surface);
        return *this;
    }

} // namespace native
