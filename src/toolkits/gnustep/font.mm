#import <AppKit/AppKit.h>
#include <native.h>
#include "globals.h"

// font_t on GNUstep: the platform handle (gnustepfont) retains an NSFont and
// lives in gnustep::font_bindings, keyed by the font's opaque uint32_t id.

namespace
{
    uint32_t next_id()
    {
        static uint32_t counter = 0;
        return ++counter;
    }

    void release(uint32_t id)
    {
        auto *f = gnustep::font_bindings.from_a(id);
        if (f)
        {
            [f->ns_font release];
            delete f;
        }
        gnustep::font_bindings.unregister_by_a(id);
    }

    uint32_t register_font(NSFont *nsfont)
    {
        [nsfont retain];
        auto *h = new gnustep::gnustepfont();
        h->ns_font = nsfont;
        uint32_t id = next_id();
        gnustep::font_bindings.register_pair(id, h);
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
    CGFloat sz = (spec.size == 0) ? [NSFont systemFontSize] : (CGFloat)spec.size;
    NSFont *nsfont = nil;

    if (spec.name.empty())
    {
        nsfont = spec.bold
            ? [NSFont boldSystemFontOfSize:sz]
            : [NSFont systemFontOfSize:sz];
    }
    else
    {
        NSString *name = [NSString stringWithUTF8String:spec.name.c_str()];
        nsfont = [NSFont fontWithName:name size:sz];
        if (!nsfont) nsfont = [NSFont systemFontOfSize:sz];
    }

    if (nsfont)
    {
        f._id = register_font(nsfont);
        f._spec = spec;
    }
    return f;
}

const font_t &font_t::stock(font_role role)
{
    static font_t s[5];
    static bool initialized = false;
    if (!initialized)
    {
        @autoreleasepool
        {
            initialized = true;

            CGFloat sz       = [NSFont systemFontSize];
            CGFloat sz_small = [NSFont smallSystemFontSize];

            auto init = [&](font_role r, NSFont *nsfont) {
                s[(int)r]._id = register_font(nsfont);
            };

            init(font_role::system,  [NSFont systemFontOfSize:sz]);
            init(font_role::fixed,   [NSFont userFixedPitchFontOfSize:sz]);
            // GNUstep may not have titleBarFontOfSize; use bold system font.
            init(font_role::title,   [NSFont boldSystemFontOfSize:sz]);
            init(font_role::small_,  [NSFont systemFontOfSize:sz_small]);
            init(font_role::control, [NSFont systemFontOfSize:sz]);
        }
    }
    return s[(int)role];
}

} // namespace native
