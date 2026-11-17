//
// Declares two eager image transformations. Chained shifts apply each
// transformation from left to right without changing the source image.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include "graphics.h"

namespace native
{
    // Describes a resize; nearest sampling preserves pixel-art edges.
    struct resize
    {
        // Require positive dimensions that fit an image and a valid sampler.
        resize(dim width, dim height,
               image_filter filter = image_filter::nearest);

        dim width;
        dim height;
        image_filter filter;
    };

    // Selects ordered 4x4 black-and-white dithering, preserving alpha.
    struct dither {};

    // Return a resized image with independent storage; leave source intact.
    img operator>>(const img &source, const resize &operation);

    // Return a dithered image of the same size; leave source intact.
    img operator>>(const img &source, dither operation);
} // namespace native
