#include <Font.h>
#include <algorithm>

#include <native.h>
#include "globals.h"

// font_t on Haiku: the platform handle (haikufont) copies a BFont value and
// lives in haiku::font_bindings, keyed by the font's opaque uint32_t id.

namespace
{
    uint32_t next_id()
    {
        static uint32_t counter = 0;
        return ++counter;
    }

    void release(uint32_t id)
    {
        auto *f = haiku::font_bindings.from_a(id);
        if (f) delete f;
        haiku::font_bindings.unregister_by_a(id);
    }

    uint32_t register_font(const BFont &bfont)
    {
        auto *h = new haiku::haikufont();
        h->bfont = bfont;
        uint32_t id = next_id();
        haiku::font_bindings.register_pair(id, h);
        return id;
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
    BFont bfont = *be_plain_font;

    if (!spec.name.empty())
        bfont.SetFamilyAndStyle(spec.name.c_str(), nullptr);
    if (spec.size > 0)
        bfont.SetSize((float)spec.size);
    if (spec.bold && spec.italic)
        bfont.SetFace(B_BOLD_FACE | B_ITALIC_FACE);
    else if (spec.bold)
        bfont.SetFace(B_BOLD_FACE);
    else if (spec.italic)
        bfont.SetFace(B_ITALIC_FACE);

    f._id = register_font(bfont);
    f._spec = spec;
    return f;
}

const font_t &font_t::stock(font_role role)
{
    static font_t s[5];
    static bool initialized = false;
    if (!initialized)
    {
        initialized = true;

        s[(int)font_role::system]._id  = register_font(*be_plain_font);
        s[(int)font_role::fixed]._id   = register_font(*be_fixed_font);
        s[(int)font_role::title]._id   = register_font(*be_bold_font);
        s[(int)font_role::control]._id = register_font(*be_plain_font);

        BFont small = *be_plain_font;
        small.SetSize(std::max(8.0f, be_plain_font->Size() * 0.85f));
        s[(int)font_role::small_]._id = register_font(small);
    }
    return s[(int)role];
}

} // namespace native
