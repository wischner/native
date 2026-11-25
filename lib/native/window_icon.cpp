//
// Fits application icons into the square sizes requested by native
// window systems without distorting their aspect ratio.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "window_icon.h"

#include <algorithm>
#include <cstddef>
#include <stdexcept>

#include <native/image_filters.h>

namespace native::detail
{
    std::unique_ptr<img> fit_window_icon(const img &source, dim edge) {
        if (edge == 0)
            throw std::invalid_argument(
                "Window icon edge must be positive.");
        const int longest = std::max<int>(source.w(), source.h());
        const auto scaled = [&](long long length) {
            return static_cast<dim>(std::max(
                1LL, (length * edge + longest / 2) / longest));
        };
        const dim width = scaled(source.w());
        const dim height = scaled(source.h());
        const bool whole = edge >= longest && edge % longest == 0;
        const img fitted = source >> resize(
            width, height,
            whole ? image_filter::nearest : image_filter::linear);

        auto square = std::make_unique<img>(edge, edge);
        const int left = (edge - width) / 2;
        const int top = (edge - height) / 2;
        for (int y = 0; y < height; ++y)
            std::copy_n(fitted.pixels() + std::size_t(y) * width, width,
                        square->pixels() +
                            std::size_t(y + top) * edge + left);
        return square;
    }
} // namespace native::detail
