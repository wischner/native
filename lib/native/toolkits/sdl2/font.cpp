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
#include <native/font.h>
#include "../../portable_font.h"
#include "globals.h"

// font_t on SDL2: the platform handle (sdl2_font) owns a TTF_Font and
// lives in linux::sdl2::font_bindings, keyed by the font's opaque
// uint32_t id. Without SDL2_ttf, stock roles use the backend's built-in
// bitmap font while portable fonts keep the shared TrueType path.

#ifdef HAVE_SDL2_TTF

namespace
{
    struct stock_font_def
    {
        native::font_role role;
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
        if (!fp)
            return {};
        char buf[1024] = {};
        if (!fgets(buf, sizeof(buf), fp)) {
            pclose(fp);
            return {};
        }
        pclose(fp);
        std::string path(buf);
        while (!path.empty() &&
               (path.back() == '\n' || path.back() == '\r' ||
                path.back() == ' '))
            path.pop_back();
        return path;
    }

    TTF_Font *open_by_pattern(const char *pattern, int size) {
        TTF_Font *f = TTF_OpenFont(pattern, size);
        if (f)
            return f;
        std::string path = fc_match(pattern);
        if (!path.empty())
            f = TTF_OpenFont(path.c_str(), size);
        return f;
    }

    TTF_Font *open_by_fallbacks(const char *const *fallbacks,
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
} // namespace

#endif // HAVE_SDL2_TTF

namespace native
{

    font_t::font_t() = default;

    font_t::font_t(font_t &&other) noexcept
        : _id(other._id)
        , _spec(std::move(other._spec)) {
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
        if (detail::release_portable_font(_id)) {
            _id = 0;
            return;
        }
#ifdef HAVE_SDL2_TTF
        if (_id) {
            release(_id);
            _id = 0;
        }
#endif
    }

    const font_t &font_t::stock(font_role role) {
        static font_t s[6];
        static bool initialized = false;
        if (!initialized) {
            initialized = true;
#ifdef HAVE_SDL2_TTF
            static const stock_font_def defs[] = {
                {font_role::system,
                 13,
                 {"Lato",
                  "Roboto",
                  "Noto Sans",
                  "Inter",
                  "Segoe UI",
                  "Helvetica Neue",
                  "sans",
                  nullptr}},
                {font_role::fixed,
                 12,
                 {"Roboto Mono",
                  "JetBrains Mono",
                  "DejaVu Sans Mono",
                  "monospace",
                  nullptr}},
                {font_role::icon_label,
                 11,
                 {"Lato",
                  "Roboto",
                  "Noto Sans",
                  "Inter",
                  "Segoe UI",
                  "sans",
                  nullptr,
                  nullptr}},
                {font_role::title,
                 13,
                 {"Lato Semibold",
                  "Roboto Medium",
                  "Roboto",
                  "Noto Sans",
                  "Segoe UI",
                  "sans",
                  nullptr,
                  nullptr}},
                {font_role::small,
                 10,
                 {"Lato",
                  "Roboto",
                  "Inter",
                  "Noto Sans",
                  "Segoe UI",
                  "sans",
                  nullptr}},
                {font_role::control,
                 12,
                 {"Lato",
                  "Roboto",
                  "Noto Sans",
                  "Inter",
                  "Segoe UI",
                  "sans",
                  nullptr}},
            };
            for (const auto &d : defs) {
                const char *chosen_pattern = nullptr;
                TTF_Font *ttf = open_by_fallbacks(
                    d.fallbacks, d.size, &chosen_pattern);
                if (ttf) {
                    s[(int)d.role]._id = register_font(ttf);
                    s[(int)d.role]._spec.family =
                        chosen_pattern ? chosen_pattern : "";
                    s[(int)d.role]._spec.size = d.size;
                }
            }
#else
            for (int index = 0; index < 6; ++index) {
                s[index]._id = static_cast<std::uint32_t>(index + 1);
            }
#endif
            for (int index = 0; index < 6; ++index) {
                if (!s[index]._id) {
                    s[index]._id =
                        0x80000000U + static_cast<std::uint32_t>(index);
                }
                if (s[index]._spec.family.empty())
                    s[index]._spec.family = "Built-in Bitmap";
                if (s[index]._spec.size == 0)
                    s[index]._spec.size = index ==
                            static_cast<int>(font_role::small)
                        ? 10
                        : 12;
                s[index]._spec.source = font_source::stock;
            }
        }
        return s[(int)role];
    }

    font_metrics font_t::get_metrics() const {
        if (detail::is_portable_font(_id))
            return detail::portable_font_metrics(_id);
        if (!_id)
            return {};
#ifdef HAVE_SDL2_TTF
        auto *binding =
            linux::sdl2::font_bindings.object_from_handle(_id);
        if (binding && binding->ttf_font) {
            const int height = TTF_FontHeight(binding->ttf_font);
            const int line_skip = TTF_FontLineSkip(binding->ttf_font);
            int min_x = 0;
            int max_x = 0;
            int min_y = 0;
            int max_y = 0;
            int advance = 0;
            TTF_GlyphMetrics(binding->ttf_font,
                             static_cast<Uint16>('W'),
                             &min_x,
                             &max_x,
                             &min_y,
                             &max_y,
                             &advance);
            const int ascent =
                std::max(1, TTF_FontAscent(binding->ttf_font));
            const int descent =
                std::max(1, -TTF_FontDescent(binding->ttf_font));
            const int leading =
                std::max(1, line_skip - height);
            return {ascent,
                    descent,
                    leading,
                    ascent + descent + leading,
                    std::max(1, advance)};
        }
#endif
        return {7, 1, 1, 9, 6};
    }

    text_metrics font_t::measure_text(const std::string &text) const {
        if (detail::is_portable_font(_id))
            return detail::measure_portable_text(_id, text);
        if (!_id)
            return {};
        const font_metrics metrics = get_metrics();
#ifdef HAVE_SDL2_TTF
        auto *binding =
            linux::sdl2::font_bindings.object_from_handle(_id);
        if (binding && binding->ttf_font && !text.empty()) {
            int width = 0;
            int height = 0;
            if (TTF_SizeUTF8(
                    binding->ttf_font, text.c_str(), &width, &height) ==
                0) {
                return {width, metrics.height, width};
            }
        }
#endif
        const int width =
            text.empty() ? 0 : static_cast<int>(text.size()) * 6 - 1;
        return {width, metrics.height, width};
    }

} // namespace native
