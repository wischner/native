//
// Implements conversion between Native RGBA pixels and X11 TrueColor
// visuals for the X11 and OpenMotif graphics backends.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "x_image.h"

#include <algorithm>
#include <cstdlib>
#include <stdexcept>

#include <X11/Xutil.h>

#include <native/graphics.h>

namespace native::detail
{
    unsigned mask_shift(unsigned long mask) {
        unsigned shift = 0;
        if (mask == 0)
            return shift;
        while ((mask & 1UL) == 0) {
            mask >>= 1;
            ++shift;
        }
        return shift;
    }

    unsigned long channel_to_mask(std::uint8_t channel,
                                  unsigned long mask) {
        if (mask == 0)
            return 0;
        const unsigned shift = mask_shift(mask);
        const unsigned long maximum = mask >> shift;
        return ((static_cast<unsigned long>(channel) * maximum +
                 127UL) /
                255UL)
               << shift;
    }

    std::uint8_t channel_from_mask(unsigned long pixel,
                                   unsigned long mask) {
        if (mask == 0)
            return 0;
        const unsigned shift = mask_shift(mask);
        const unsigned long maximum = mask >> shift;
        const unsigned long value = (pixel & mask) >> shift;
        return static_cast<std::uint8_t>(
            (value * 255UL + maximum / 2UL) / maximum);
    }

    unsigned long x_pixel(Visual *visual, rgba color) {
        return channel_to_mask(color.r, visual->red_mask) |
               channel_to_mask(color.g, visual->green_mask) |
               channel_to_mask(color.b, visual->blue_mask);
    }

    rgba rgba_from_x_pixel(Visual *visual, unsigned long pixel) {
        return rgba(channel_from_mask(pixel, visual->red_mask),
                    channel_from_mask(pixel, visual->green_mask),
                    channel_from_mask(pixel, visual->blue_mask),
                    255);
    }

    XImage *x_image_from_rgba(Display *display,
                              const rgba *pixels,
                              dim width,
                              dim height) {
        const int screen = DefaultScreen(display);
        Visual *visual = DefaultVisual(display, screen);
        XImage *image = XCreateImage(display,
                                     visual,
                                     DefaultDepth(display, screen),
                                     ZPixmap,
                                     0,
                                     nullptr,
                                     width,
                                     height,
                                     32,
                                     0);
        if (!image)
            throw std::runtime_error("X11: unable to create image");
        image->data = static_cast<char *>(std::calloc(
            static_cast<std::size_t>(image->bytes_per_line), height));
        if (!image->data) {
            XDestroyImage(image);
            throw std::bad_alloc();
        }
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                XPutPixel(image,
                          x,
                          y,
                          x_pixel(visual, pixels[y * width + x]));
            }
        }
        return image;
    }

    void blend_x_image(Display *display,
                       Drawable drawable,
                       GC gc,
                       const img &source,
                       point destination,
                       const rect &clip,
                       size drawable_size) {
        const int left =
            std::max<int>({destination.x, clip.p.x, 0});
        const int top =
            std::max<int>({destination.y, clip.p.y, 0});
        const int right = std::min<int>(
            {destination.x + source.w(), clip.x2(), drawable_size.w});
        const int bottom = std::min<int>(
            {destination.y + source.h(), clip.y2(), drawable_size.h});
        if (left >= right || top >= bottom)
            return;

        const unsigned width = static_cast<unsigned>(right - left);
        const unsigned height = static_cast<unsigned>(bottom - top);
        XImage *target = XGetImage(display,
                                   drawable,
                                   left,
                                   top,
                                   width,
                                   height,
                                   AllPlanes,
                                   ZPixmap);
        if (!target)
            return;

        Visual *visual = DefaultVisual(display, DefaultScreen(display));
        for (int y = top; y < bottom; ++y) {
            for (int x = left; x < right; ++x) {
                const rgba foreground = source.pixels()[
                    (y - destination.y) * source.w() +
                    (x - destination.x)];
                if (foreground.a == 0)
                    continue;

                rgba result = foreground;
                if (foreground.a != 255) {
                    const rgba background = rgba_from_x_pixel(
                        visual, XGetPixel(target, x - left, y - top));
                    const unsigned alpha = foreground.a;
                    const unsigned inverse = 255U - alpha;
                    result.r = static_cast<std::uint8_t>(
                        (foreground.r * alpha +
                         background.r * inverse + 127U) /
                        255U);
                    result.g = static_cast<std::uint8_t>(
                        (foreground.g * alpha +
                         background.g * inverse + 127U) /
                        255U);
                    result.b = static_cast<std::uint8_t>(
                        (foreground.b * alpha +
                         background.b * inverse + 127U) /
                        255U);
                    result.a = 255;
                }
                XPutPixel(target,
                          x - left,
                          y - top,
                          x_pixel(visual, result));
            }
        }

        XPutImage(display,
                  drawable,
                  gc,
                  target,
                  0,
                  0,
                  left,
                  top,
                  width,
                  height);
        XDestroyImage(target);
    }
} // namespace native::detail
