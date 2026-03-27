#include <SDL2/SDL.h>
#ifdef HAVE_SDL2_TTF
#include <SDL2/SDL_ttf.h>
#endif
#include <cstdio>
#include <string>

#include <native.h>
#include "globals.h"

// font_t on SDL2: the platform handle (sdl2font) owns a TTF_Font and
// lives in sdl::font_bindings, keyed by the font's opaque uint32_t id.
// When HAVE_SDL2_TTF is not defined, all methods produce invalid font_t
// objects and draw_text remains a no-op.

#ifdef HAVE_SDL2_TTF

namespace
{
    uint32_t next_id()
    {
        static uint32_t counter = 0;
        return ++counter;
    }

    void release(uint32_t id)
    {
        auto *f = sdl::font_bindings.from_a(id);
        if (f)
        {
            TTF_CloseFont(f->ttf_font);
            delete f;
        }
        sdl::font_bindings.unregister_by_a(id);
    }

    uint32_t register_font(TTF_Font *ttf_font)
    {
        auto *h = new sdl::sdl2font();
        h->ttf_font = ttf_font;
        uint32_t id = next_id();
        sdl::font_bindings.register_pair(id, h);
        return id;
    }

    // Query the system font path via fontconfig (fc-match).
    std::string fc_match(const char *pattern)
    {
        std::string cmd = "fc-match --format='%{file}' '";
        cmd += pattern;
        cmd += "' 2>/dev/null";
        FILE *fp = popen(cmd.c_str(), "r");
        if (!fp) return {};
        char buf[1024] = {};
        if (!fgets(buf, sizeof(buf), fp))
        {
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

    TTF_Font *open_by_pattern(const char *pattern, int size)
    {
        TTF_Font *f = TTF_OpenFont(pattern, size);
        if (f) return f;
        std::string path = fc_match(pattern);
        if (!path.empty())
            f = TTF_OpenFont(path.c_str(), size);
        return f;
    }
}

#endif // HAVE_SDL2_TTF

namespace native
{

font_t::font_t() = default;

font_t::font_t(font_t &&other) noexcept
    : _id(other._id), _spec(std::move(other._spec))
{
    other._id = 0;
}

font_t &font_t::operator=(font_t &&other) noexcept
{
    if (this != &other)
    {
        std::swap(_id, other._id);
        _spec = std::move(other._spec);
    }
    return *this;
}

font_t::~font_t()
{
#ifdef HAVE_SDL2_TTF
    if (_id)
    {
        release(_id);
        _id = 0;
    }
#endif
}

font_t font_t::create(const font_spec &spec)
{
    font_t f;
#ifdef HAVE_SDL2_TTF
    int size = (spec.size == 0) ? 12 : spec.size;
    TTF_Font *ttf = open_by_pattern(spec.name.c_str(), size);
    if (ttf)
    {
        f._id = register_font(ttf);
        f._spec = spec;
    }
#endif
    return f;
}

const font_t &font_t::stock(font_role role)
{
    static font_t s[5];
    static bool initialized = false;
    if (!initialized)
    {
        initialized = true;
#ifdef HAVE_SDL2_TTF
        struct { font_role role; const char *pattern; int size; } defs[] = {
            { font_role::system,  "sans",      12 },
            { font_role::fixed,   "monospace", 12 },
            { font_role::title,   "sans:bold", 12 },
            { font_role::small_,  "sans",      10 },
            { font_role::control, "sans",      11 },
        };
        for (auto &d : defs)
        {
            TTF_Font *ttf = open_by_pattern(d.pattern, d.size);
            if (ttf)
            {
                s[(int)d.role]._id = register_font(ttf);
                s[(int)d.role]._spec.name = d.pattern;
                s[(int)d.role]._spec.size = d.size;
            }
        }
#endif
    }
    return s[(int)role];
}

} // namespace native
