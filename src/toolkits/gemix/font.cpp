#include <array>

#include <native.h>

namespace
{
    uint32_t next_font_id()
    {
        static uint32_t counter = 1;
        return counter++;
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
            _id = other._id;
            _spec = std::move(other._spec);
            other._id = 0;
        }
        return *this;
    }

    font_t::~font_t() = default;

    font_t font_t::create(const font_spec &spec)
    {
        font_t font;
        font._id = next_font_id();
        font._spec = spec;
        return font;
    }

    const font_t &font_t::stock(font_role role)
    {
        static std::array<font_t, 5> fonts;
        static bool initialized = false;

        if (!initialized)
        {
            initialized = true;
            for (auto &font : fonts)
                font._id = next_font_id();

            fonts[static_cast<int>(font_role::system)]._spec.name = "GEM System";
            fonts[static_cast<int>(font_role::fixed)]._spec.name = "GEM Fixed";
            fonts[static_cast<int>(font_role::title)]._spec.name = "GEM Title";
            fonts[static_cast<int>(font_role::small_)]._spec.name = "GEM Small";
            fonts[static_cast<int>(font_role::control)]._spec.name = "GEM Control";
        }

        return fonts[static_cast<int>(role)];
    }
}
