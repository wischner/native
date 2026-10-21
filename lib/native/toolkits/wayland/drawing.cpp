//
// Converts portable images and renders Unicode text with Cairo and
// Pango.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include <pango/pangocairo.h>
#include <vector>
namespace linux::wayland
{
    void text(cairo_t *cr,
              const std::string &value,
              native::point p,
              const native::font_t *font,
              native::rgba color) {
        auto *layout = pango_cairo_create_layout(cr);
        auto *description = pango_font_description_new();
        const auto &spec =
            (font ? *font
                  : native::font_t::stock(native::font_role::system))
                .spec();
        pango_font_description_set_family(description,
                                          spec.family.c_str());
        pango_font_description_set_absolute_size(
            description, spec.size * PANGO_SCALE);
        pango_layout_set_font_description(layout, description);
        pango_layout_set_text(layout, value.c_str(), value.size());
        cairo_set_source_rgba(cr,
                              color.r / 255.,
                              color.g / 255.,
                              color.b / 255.,
                              color.a / 255.);
        cairo_move_to(cr, p.x, p.y);
        pango_cairo_show_layout(cr, layout);
        pango_font_description_free(description);
        g_object_unref(layout);
    }
    void
    image(cairo_t *cr, const native::img &source, native::point p) {
        if (!source.w() || !source.h())
            return;
        auto *surface = cairo_image_surface_create(
            CAIRO_FORMAT_ARGB32, source.w(), source.h());
        auto *pixels = reinterpret_cast<std::uint32_t *>(
            cairo_image_surface_get_data(surface));
        const int stride = cairo_image_surface_get_stride(surface) / 4;
        for (int y = 0; y < source.h(); ++y)
            for (int x = 0; x < source.w(); ++x) {
                const auto i =
                    static_cast<std::size_t>(y) * source.w() + x;
                const auto c = source.pixels()[i];
                pixels[y * stride + x] =
                    (unsigned(c.a) << 24) |
                    ((unsigned(c.r) * c.a / 255) << 16) |
                    ((unsigned(c.g) * c.a / 255) << 8) |
                    (unsigned(c.b) * c.a / 255);
            }
        cairo_surface_mark_dirty(surface);
        cairo_set_source_surface(cr, surface, p.x, p.y);
        cairo_paint(cr);
        cairo_surface_destroy(surface);
    }
} // namespace linux::wayland
