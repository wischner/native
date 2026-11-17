//
// Applies independent resize and black-and-white ordered-dither steps.
// Results own their pixels; no pipeline or deferred work is retained.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/image_filters.h>

#include <limits>
#include <stdexcept>

#include "software_image.h"

namespace native
{
    resize::resize(dim w, dim h, image_filter sampling)
        : width(w), height(h), filter(sampling) {
        if (!width || !height ||
            width > std::numeric_limits<coord>::max() ||
            height > std::numeric_limits<coord>::max())
            throw std::invalid_argument("resize: invalid image dimensions");
        if (filter != image_filter::nearest && filter != image_filter::linear)
            throw std::invalid_argument("resize: invalid sampling filter");
    }

    img operator>>(const img &source, const resize &operation) {
        // Validate descriptor values again because they are public values.
        const resize checked(operation.width, operation.height,
                             operation.filter);
        auto pixels = std::make_unique<rgba[]>(
            std::size_t(checked.width) * checked.height);
        const rect bounds(0, 0, source.w(), source.h());
        const double sx = double(source.w()) / checked.width;
        const double sy = double(source.h()) / checked.height;
        for (int y = 0; y < checked.height; ++y)
            for (int x = 0; x < checked.width; ++x)
                pixels[std::size_t(y) * checked.width + x] =
                    detail::sample_image(source, bounds,
                        (x + 0.5) * sx - 0.5, (y + 0.5) * sy - 0.5,
                        checked.filter);
        return img(checked.width, checked.height, std::move(pixels));
    }

    img operator>>(const img &source, dither) {
        constexpr unsigned bayer[16] = {
            0, 8, 2, 10, 12, 4, 14, 6,
            3, 11, 1, 9, 15, 7, 13, 5};
        auto pixels = std::make_unique<rgba[]>(
            std::size_t(source.w()) * source.h());
        for (int y = 0; y < source.h(); ++y) {
            for (int x = 0; x < source.w(); ++x) {
                const auto index = std::size_t(y) * source.w() + x;
                const rgba original = source.pixels()[index];
                const unsigned luminance =
                    (77U * original.r + 150U * original.g +
                     29U * original.b) >> 8;
                const auto value = static_cast<std::uint8_t>(
                    luminance >= bayer[(y & 3) * 4 + (x & 3)] * 16 + 8
                        ? 255 : 0);
                pixels[index] = rgba(value, value, value, original.a);
            }
        }
        return img(source.w(), source.h(), std::move(pixels));
    }
} // namespace native
