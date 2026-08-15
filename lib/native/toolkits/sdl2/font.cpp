//
// Implements the SDL2 font-resource backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <SDL2/SDL.h>
#ifdef HAVE_SDL2_TTF
#include <SDL2/SDL_ttf.h>
#endif
#include <cstdio>
#include <algorithm>
#include <string>

#include <native.h>
#include "globals.h"

// font_t on SDL2: the platform handle (sdl2_font) owns a TTF_Font and
// lives in linux::sdl2::font_bindings, keyed by the font's opaque uint32_t id.
// When HAVE_SDL2_TTF is not defined, all methods produce invalid font_t
// objects and draw_text remains a no-op.

#ifdef HAVE_SDL2_TTF

namespace
{
    struct stock_font_def
    {
        font_role role;
        int size;
        const char *fallbacks[8];
    };

    uint32_t next_id() {
        static uint32_t counter = 0;
        return ++counter;
    }

    void release(uint32_t id) {
        auto *f = linux::sdl2::font_bindings.object_from_handle(id);
        if (f) {
            TTF_CloseFont(f->ttf_font);
            delete f;
        }
        linux::sdl2::font_bindings.unregister_by_handle(id);
    }

    uint32_t register_font(TTF_Font *ttf_font) {
        auto *h = new linux::sdl2::sdl2_font();
        h->ttf_font = ttf_font;
        uint32_t id = next_id();
        linux::sdl2::font_bindings.register_pair(id, h);
        return id;
    }

    // Query the system font path via fontconfig (fc-match).
    std::string fc_match(const char *pattern) {
        std::string cmd = "fc-match --format='%{file}' '";
        cmd += pattern;
        cmd += "' 2>/dev/null";
        FILE *fp = popen(cmd.c_str(), "r");
        if (!fp) return {};
        char buf[1024] = {};
        if (!fgets(buf, sizeof(buf), fp)) {
            pclose(fp);
            return {};
        }
        pclose(fp);
        std::string path(buf);
        while (!path.empty() &&
               (path.back() == '\n' || path.back() == '\r' || path.back() == ' '))
            path.pop_back();
        return path;
    }

    TTF_Font *open_by_pattern(const char *pattern, int size) {
        TTF_Font *f = TTF_OpenFont(pattern, size);
        if (f) return f;
        std::string path = fc_match(pattern);
        if (!path.empty())
            f = TTF_OpenFont(path.c_str(), size);
        return f;
    }

    TTF_Font *open_by_fallbacks(
        const char *const *fallbacks,
        int size,
        const char **chosen_pattern) {
        for (int i = 0; fallbacks[i]; ++i) {
            TTF_Font *f = open_by_pattern(fallbacks[i], size);
            if (f) {
                if (chosen_pattern)
                    *chosen_pattern = fallbacks[i];
                return f;
            }
        }
        return nullptr;
    }
}

#endif // HAVE_SDL2_TTF

namespace native
{

font_t::font_t() = default;

font_t::font_t(font_t &&other) noexcept
    : _id(other._id), _spec(std::move(other._spec)) {
    other._id = 0;
}

font_t &font_t::operator=(font_t &&other) noexcept {
    if (this != &other) {
        std::swap(_id, other._id);
        _spec = std::move(other._spec);
    }
    return *this;
}

font_t::~font_t() {
#ifdef HAVE_SDL2_TTF
    if (_id) {
        release(_id);
        _id = 0;
    }
#endif
}

font_t font_t::create(const font_spec &spec) {
    font_t f;
#ifdef HAVE_SDL2_TTF
    int size = (spec.size == 0) ? 12 : spec.size;
    TTF_Font *ttf = open_by_pattern(spec.name.c_str(), size);
    if (ttf) {
        f._id = register_font(ttf);
        f._spec = spec;
    }
#else
    (void)spec;
#endif
    return f;
}

const font_t &font_t::stock(font_role role) {
    static font_t s[5];
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
#ifdef HAVE_SDL2_TTF
        static const stock_font_def defs[] = {
            {font_role::system,
             12,
             {"Roboto", "Noto Sans", "Inter", "Segoe UI",
              "Helvetica Neue", "Arial", "sans", nullptr}},
            {font_role::fixed,
             12,
             {"Roboto Mono", "JetBrains Mono", "DejaVu Sans Mono",
              "monospace", nullptr}},
            {font_role::title,
             12,
             {"Roboto Medium", "Roboto", "Noto Sans", "Segoe UI",
              "sans", nullptr}},
            {font_role::small_,
             10,
             {"Roboto", "Inter", "Noto Sans", "Segoe UI", "Arial",
              "sans", nullptr}},
            {font_role::control,
             11,
             {"Roboto", "Noto Sans", "Inter", "Segoe UI", "sans",
              nullptr}},
        };
        for (const auto &d : defs) {
            const char *chosen_pattern = nullptr;
            TTF_Font *ttf = open_by_fallbacks(
                d.fallbacks, d.size, &chosen_pattern);
            if (ttf) {
                s[(int)d.role]._id = register_font(ttf);
                s[(int)d.role]._spec.name = chosen_pattern ? chosen_pattern : "";
                s[(int)d.role]._spec.size = d.size;
            }
        }
#else
        for (int index = 0; index < 5; ++index) {
            s[index]._id = static_cast<std::uint32_t>(index + 1);
        }
#endif
        for (int index = 0; index < 5; ++index) {
            if (!s[index]._id) {
                s[index]._id = 0x80000000U +
                    static_cast<std::uint32_t>(index);
            }
        }
    }
    return s[(int)role];
}

font_metrics font_t::get_metrics() const {
    if (!_id)
        return {};
#ifdef HAVE_SDL2_TTF
    auto *binding = linux::sdl2::font_bindings.object_from_handle(_id);
    if (binding && binding->ttf_font) {
        const int height = TTF_FontHeight(binding->ttf_font);
        const int line_skip = TTF_FontLineSkip(binding->ttf_font);
        int min_x = 0;
        int max_x = 0;
        int min_y = 0;
        int max_y = 0;
        int advance = 0;
        TTF_GlyphMetrics(
            binding->ttf_font,
            static_cast<Uint16>('W'),
            &min_x,
            &max_x,
            &min_y,
            &max_y,
            &advance);
        return {
            TTF_FontAscent(binding->ttf_font),
            std::max(0, -TTF_FontDescent(binding->ttf_font)),
            std::max(0, line_skip - height),
            line_skip,
            advance};
    }
#endif
    return {7, 0, 0, 7, 6};
}

text_metrics font_t::measure_text(const std::string &text) const {
    if (!_id)
        return {};
    const font_metrics metrics = get_metrics();
#ifdef HAVE_SDL2_TTF
    auto *binding = linux::sdl2::font_bindings.object_from_handle(_id);
    if (binding && binding->ttf_font && !text.empty()) {
        int width = 0;
        int height = 0;
        if (TTF_SizeUTF8(
                binding->ttf_font,
                text.c_str(),
                &width,
                &height) == 0) {
            return {width, metrics.height, width};
        }
    }
#endif
    const int width = text.empty()
        ? 0
        : static_cast<int>(text.size()) * 6 - 1;
    return {width, metrics.height, width};
}

} // namespace native
