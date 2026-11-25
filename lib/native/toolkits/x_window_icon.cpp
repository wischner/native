//
// Converts portable RGBA icons into EWMH ARGB properties and ICCCM
// pixmaps. Color pixmaps use the default TrueColor visual; any other
// visual receives the classic dithered bitmap that managers expect.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "x_window_icon.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <algorithm>
#include <bit>
#include <cstdlib>
#include <vector>

#include <native/image_filters.h>
#include "../window_icon.h"

namespace
{
    // Scale one 8-bit channel into a TrueColor visual mask.
    unsigned long channel(std::uint8_t value, unsigned long mask) {
        if (!mask)
            return 0;
        const int shift = std::countr_zero(mask);
        const unsigned long maximum = mask >> shift;
        return ((value * maximum + 127) / 255) << shift;
    }

    // Pack one bitmap row per scanline, least significant bit first.
    template <typename predicate>
    std::vector<char> bitmap_bits(const native::img &image,
                                  predicate set) {
        const int stride = (image.w() + 7) / 8;
        std::vector<char> bits(std::size_t(stride) * image.h(), 0);
        for (int y = 0; y < image.h(); ++y)
            for (int x = 0; x < image.w(); ++x)
                if (set(image.pixels()[std::size_t(y) * image.w() + x]))
                    bits[std::size_t(y) * stride + x / 8] |=
                        static_cast<char>(1 << (x % 8));
        return bits;
    }

    // Upload a default-depth TrueColor pixmap through an XImage.
    Pixmap color_pixmap(Display *display, int screen,
                        const native::img &image) {
        Visual *visual = DefaultVisual(display, screen);
        const int depth = DefaultDepth(display, screen);
        XImage *pixels = XCreateImage(
            display, visual, static_cast<unsigned>(depth), ZPixmap, 0,
            nullptr, static_cast<unsigned>(image.w()),
            static_cast<unsigned>(image.h()), 32, 0);
        if (!pixels)
            return None;
        pixels->data = static_cast<char *>(
            std::calloc(std::size_t(pixels->bytes_per_line),
                        std::size_t(image.h())));
        if (!pixels->data) {
            XDestroyImage(pixels);
            return None;
        }
        for (int y = 0; y < image.h(); ++y)
            for (int x = 0; x < image.w(); ++x) {
                const auto &value =
                    image.pixels()[std::size_t(y) * image.w() + x];
                XPutPixel(pixels, x, y,
                          channel(value.r, visual->red_mask) |
                              channel(value.g, visual->green_mask) |
                              channel(value.b, visual->blue_mask));
            }
        const Pixmap pixmap = XCreatePixmap(
            display, RootWindow(display, screen),
            static_cast<unsigned>(image.w()),
            static_cast<unsigned>(image.h()),
            static_cast<unsigned>(depth));
        GC gc = XCreateGC(display, pixmap, 0, nullptr);
        XPutImage(display, pixmap, gc, pixels, 0, 0, 0, 0,
                  static_cast<unsigned>(image.w()),
                  static_cast<unsigned>(image.h()));
        XFreeGC(display, gc);
        XDestroyImage(pixels);
        return pixmap;
    }

    // Upload packed depth-1 bits as a pixmap on the screen root.
    Pixmap bitmap(Display *display, int screen,
                  const native::img &image,
                  const std::vector<char> &bits) {
        return XCreateBitmapFromData(
            display, RootWindow(display, screen), bits.data(),
            static_cast<unsigned>(image.w()),
            static_cast<unsigned>(image.h()));
    }
} // namespace

namespace native::detail
{
    dim x_icon_edge(Display *display, int screen, dim preferred) {
        XIconSize *sizes = nullptr;
        int count = 0;
        int edge = preferred;
        // WM_ICON_SIZE lists acceptable ranges, not a preference: keep
        // the toolkit's size and move it only into the nearest range.
        if (XGetIconSizes(display, RootWindow(display, screen), &sizes,
                          &count) && sizes) {
            int best = 0;
            for (int i = 0; i < count; ++i) {
                const XIconSize &range = sizes[i];
                const int low = std::max(
                    {1, range.min_width, range.min_height});
                const int high =
                    std::min(range.max_width, range.max_height);
                if (high < low)
                    continue;
                const int step = std::max(
                    {1, range.width_inc, range.height_inc});
                int candidate = std::clamp(int(preferred), low, high);
                candidate =
                    low + (candidate - low + step / 2) / step * step;
                if (candidate > high)
                    candidate -= step;
                if (!best || std::abs(candidate - preferred) <
                                 std::abs(best - preferred))
                    best = candidate;
            }
            XFree(sizes);
            if (best)
                edge = best;
        }
        return static_cast<dim>(std::clamp(edge, 1, 256));
    }

    x_icon_pixmaps create_x_icon_pixmaps(Display *display, int screen,
                                         const img &icon, dim edge) {
        const auto square = fit_window_icon(icon, edge);
        const bool color =
            DefaultVisual(display, screen)->c_class == TrueColor &&
            DefaultDepth(display, screen) > 1;
        x_icon_pixmaps result;
        if (color) {
            result.image = color_pixmap(display, screen, *square);
            result.depth = DefaultDepth(display, screen);
        } else {
            const img mono = *square >> dither();
            result.image = bitmap(display, screen, mono,
                bitmap_bits(mono, [](const rgba &value) {
                    return value.a >= 128 && value.r < 128;
                }));
            result.depth = 1;
        }
        result.mask = bitmap(display, screen, *square,
            bitmap_bits(*square, [](const rgba &value) {
                return value.a >= 128;
            }));
        if (!result.image || !result.mask)
            free_x_icon_pixmaps(display, result);
        return result;
    }

    void free_x_icon_pixmaps(Display *display,
                             x_icon_pixmaps &pixmaps) {
        if (display && pixmaps.image)
            XFreePixmap(display, pixmaps.image);
        if (display && pixmaps.mask)
            XFreePixmap(display, pixmaps.mask);
        pixmaps = {};
    }

    void set_x_net_wm_icon(Display *display, Window window,
                           const img *icon) {
        if (!display || !window)
            return;
        const Atom property =
            XInternAtom(display, "_NET_WM_ICON", False);
        if (!icon) {
            XDeleteProperty(display, window, property);
            return;
        }
        // Format-32 properties are transferred as C long values.
        std::vector<unsigned long> data;
        for (const dim edge : {16, 32, 48, 64, 128}) {
            const auto square = fit_window_icon(*icon, edge);
            data.push_back(edge);
            data.push_back(edge);
            const rgba *pixel = square->pixels();
            for (std::size_t i = 0; i < std::size_t(edge) * edge; ++i)
                data.push_back(
                    (static_cast<unsigned long>(pixel[i].a) << 24) |
                    (static_cast<unsigned long>(pixel[i].r) << 16) |
                    (static_cast<unsigned long>(pixel[i].g) << 8) |
                    static_cast<unsigned long>(pixel[i].b));
        }
        XChangeProperty(display, window, property, XA_CARDINAL, 32,
                        PropModeReplace,
                        reinterpret_cast<unsigned char *>(data.data()),
                        static_cast<int>(data.size()));
    }

    void clear_x_icon_hints(Display *display, Window window) {
        if (!display || !window)
            return;
        XWMHints *hints = XGetWMHints(display, window);
        if (!hints)
            return;
        hints->flags &= ~(IconPixmapHint | IconMaskHint);
        hints->icon_pixmap = None;
        hints->icon_mask = None;
        XSetWMHints(display, window, hints);
        XFree(hints);
    }
} // namespace native::detail
