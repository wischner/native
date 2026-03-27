#include <windows.h>
#include <cstring>

#include <native.h>
#include "globals.h"

// font_t on Windows: the platform handle (winfont) owns an HFONT and lives
// in win::font_bindings, keyed by the font's opaque uint32_t id.

namespace
{
    uint32_t next_id()
    {
        static uint32_t counter = 0;
        return ++counter;
    }

    void release(uint32_t id)
    {
        auto *f = win::font_bindings.from_a(id);
        if (f)
        {
            if (f->hfont) DeleteObject(f->hfont);
            delete f;
        }
        win::font_bindings.unregister_by_a(id);
    }
}

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
    if (_id)
    {
        release(_id);
        _id = 0;
    }
}

font_t font_t::create(const font_spec &spec)
{
    font_t f;
    int height = (spec.size == 0) ? 0 : -spec.size;
    HFONT hfont = CreateFontA(
        height, 0, 0, 0,
        spec.bold ? FW_BOLD : FW_NORMAL,
        spec.italic ? TRUE : FALSE,
        FALSE, FALSE,
        DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,
        DEFAULT_PITCH | FF_DONTCARE,
        spec.name.empty() ? nullptr : spec.name.c_str());
    if (!hfont) return f;

    auto *h = new win::winfont();
    h->hfont = hfont;
    f._id = next_id();
    f._spec = spec;
    win::font_bindings.register_pair(f._id, h);
    return f;
}

const font_t &font_t::stock(font_role role)
{
    static font_t s[5];
    static bool initialized = false;
    if (!initialized)
    {
        initialized = true;

        NONCLIENTMETRICSA ncm = {};
        ncm.cbSize = sizeof(ncm);
        SystemParametersInfoA(SPI_GETNONCLIENTMETRICSA, sizeof(ncm), &ncm, 0);

        auto make = [](const LOGFONTA &lf) {
            font_t f;
            auto *h = new win::winfont();
            h->hfont = CreateFontIndirectA(&lf);
            f._id = next_id();
            f._spec.name = lf.lfFaceName;
            f._spec.size = (lf.lfHeight < 0) ? -lf.lfHeight : lf.lfHeight;
            f._spec.bold = (lf.lfWeight >= FW_BOLD);
            f._spec.italic = (lf.lfItalic != 0);
            win::font_bindings.register_pair(f._id, h);
            return f;
        };

        s[(int)font_role::system]  = make(ncm.lfMessageFont);
        s[(int)font_role::title]   = make(ncm.lfCaptionFont);
        s[(int)font_role::small_]  = make(ncm.lfSmCaptionFont);
        s[(int)font_role::control] = make(ncm.lfMenuFont);

        LOGFONTA fixed = {};
        fixed.lfHeight = ncm.lfMessageFont.lfHeight;
        fixed.lfWeight = FW_NORMAL;
        fixed.lfCharSet = DEFAULT_CHARSET;
        fixed.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
        strcpy_s(fixed.lfFaceName, LF_FACESIZE, "Courier New");
        s[(int)font_role::fixed] = make(fixed);
    }
    return s[(int)role];
}

} // namespace native
