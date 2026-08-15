//
// Converts Native RGBA pixels to and from an X11 TrueColor visual.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstdlib>
#include <stdexcept>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <native/geometry.h>

namespace native::detail
{
    inline unsigned mask_shift(unsigned long mask) {
        unsigned shift = 0;
        if (mask == 0)
            return shift;
        while ((mask & 1UL) == 0) {
            mask >>= 1;
            ++shift;
        }
        return shift;
    }

    inline unsigned long channel_to_mask(
        std::uint8_t channel,
        unsigned long mask) {
        if (mask == 0)
            return 0;
        const unsigned shift = mask_shift(mask);
        const unsigned long maximum = mask >> shift;
        return ((static_cast<unsigned long>(channel) * maximum + 127UL) /
                255UL)
               << shift;
    }

    inline std::uint8_t channel_from_mask(
        unsigned long pixel,
        unsigned long mask) {
        if (mask == 0)
            return 0;
        const unsigned shift = mask_shift(mask);
        const unsigned long maximum = mask >> shift;
        const unsigned long value = (pixel & mask) >> shift;
        return static_cast<std::uint8_t>(
            (value * 255UL + maximum / 2UL) / maximum);
    }

    inline unsigned long x_pixel(Visual *visual, rgba color) {
        return channel_to_mask(color.r, visual->red_mask) |
               channel_to_mask(color.g, visual->green_mask) |
               channel_to_mask(color.b, visual->blue_mask);
    }

    inline rgba rgba_from_x_pixel(Visual *visual, unsigned long pixel) {
        return rgba(
            channel_from_mask(pixel, visual->red_mask),
            channel_from_mask(pixel, visual->green_mask),
            channel_from_mask(pixel, visual->blue_mask),
            255);
    }

    inline XImage *x_image_from_rgba(
        Display *display,
        const rgba *pixels,
        dim width,
        dim height) {
        const int screen = DefaultScreen(display);
        Visual *visual = DefaultVisual(display, screen);
        XImage *image = XCreateImage(
            display,
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
            static_cast<std::size_t>(image->bytes_per_line),
            height));
        if (!image->data) {
            XDestroyImage(image);
            throw std::bad_alloc();
        }
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                XPutPixel(
                    image,
                    x,
                    y,
                    x_pixel(visual, pixels[y * width + x]));
            }
        }
        return image;
    }
}
