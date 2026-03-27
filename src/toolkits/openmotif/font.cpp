#include <Xm/Xm.h>
#include <X11/Xlib.h>
#include <initializer_list>

#include <native.h>
#include "globals.h"

// font_t on OpenMotif: same X11 core font strategy, using motif::font_bindings.

namespace
{
    uint32_t next_id()
    {
        static uint32_t counter = 0;
        return ++counter;
    }

    void release(uint32_t id)
    {
        auto *f = motif::font_bindings.from_a(id);
        if (f)
        {
            if (f->owned && f->display && f->xfont)
                XUnloadFont(f->display, f->xfont);
            delete f;
        }
        motif::font_bindings.unregister_by_a(id);
    }

    uint32_t register_font(Display *display, Font xfont, bool owned)
    {
        auto *h = new motif::motiffont();
        h->display = display;
        h->xfont   = xfont;
        h->owned   = owned;
        uint32_t id = next_id();
        motif::font_bindings.register_pair(id, h);
        return id;
    }

    Font try_load(Display *display, std::initializer_list<const char *> names)
    {
        for (const char *name : names)
        {
            Font f = XLoadFont(display, name);
            if (f) return f;
        }
        return 0;
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
    Display *display = motif::cached_display;
    if (!display) return f;

    Font xfont = spec.name.empty()
        ? try_load(display, { "-*-helvetica-medium-r-normal-*-12-*-*-*-*-*-*-*", "fixed" })
        : XLoadFont(display, spec.name.c_str());

    if (!xfont) return f;
    f._id = register_font(display, xfont, true);
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

        Display *display = motif::cached_display;
        if (!display) return s[(int)role];

        const char *x_font = XGetDefault(display, "*", "font");
        Font system_f = x_font ? XLoadFont(display, x_font) : 0;
        if (!system_f)
            system_f = try_load(display, {
                "-*-helvetica-medium-r-normal-*-12-*-*-*-*-*-*-*",
                "-adobe-helvetica-medium-r-normal--12-120-75-75-p-67-iso8859-1",
                "fixed"
            });

        Font fixed_f = try_load(display, {
            "-misc-fixed-medium-r-normal--13-120-75-75-c-70-iso8859-1",
            "fixed"
        });

        s[(int)font_role::system]._id  = register_font(display, system_f, false);
        s[(int)font_role::fixed]._id   = register_font(display, fixed_f,  false);
        s[(int)font_role::title]._id   = register_font(display, system_f, false);
        s[(int)font_role::small_]._id  = register_font(display, system_f, false);
        s[(int)font_role::control]._id = register_font(display, system_f, false);

        s[(int)font_role::system]._spec.name  = x_font ? x_font : "fixed";
        s[(int)font_role::fixed]._spec.name   = "-misc-fixed-medium-r-normal--13-120-75-75-c-70-iso8859-1";
        s[(int)font_role::title]._spec.name   = s[(int)font_role::system]._spec.name;
        s[(int)font_role::small_]._spec.name  = s[(int)font_role::system]._spec.name;
        s[(int)font_role::control]._spec.name = s[(int)font_role::system]._spec.name;
    }
    return s[(int)role];
}

} // namespace native
