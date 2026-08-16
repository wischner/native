//
// Declares conversion between Native RGBA pixels and X11 TrueColor
// visuals for the X11 and OpenMotif graphics backends.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <X11/Xlib.h>

#include <native/geometry.h>

namespace native
{
    class img;
}

namespace native::detail
{
    // Return the low-bit shift of an X11 visual channel mask.
    unsigned mask_shift(unsigned long mask);

    // Scale an eight-bit channel into an X11 visual channel mask.
    unsigned long channel_to_mask(std::uint8_t channel,
                                  unsigned long mask);

    // Scale an X11 visual channel mask into an eight-bit channel.
    std::uint8_t channel_from_mask(unsigned long pixel,
                                   unsigned long mask);

    // Convert one Native color into an X11 TrueColor pixel value.
    unsigned long x_pixel(Visual *visual, rgba color);

    // Convert one X11 TrueColor pixel value into an opaque Native
    // color.
    rgba rgba_from_x_pixel(Visual *visual, unsigned long pixel);

    // Allocate an XImage populated from top-to-bottom RGBA pixels.
    XImage *x_image_from_rgba(Display *display,
                              const rgba *pixels,
                              dim width,
                              dim height);

    // Composite an RGBA image over an X11 drawable. The drawable is
    // assumed to use the display's default TrueColor visual.
    void blend_x_image(Display *display,
                       Drawable drawable,
                       GC gc,
                       const img &source,
                       point destination,
                       const rect &clip,
                       size drawable_size);
} // namespace native::detail
