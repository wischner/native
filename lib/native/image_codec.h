//
// Declares the platform image-codec boundary used by native::img.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include <native/graphics.h>

namespace native::detail
{
    struct decoded_image
    {
        dim width = 0;
        dim height = 0;
        std::unique_ptr<rgba[]> pixels;
    };

    decoded_image decode_image(const std::uint8_t *data,
                               std::size_t size);

    std::vector<std::uint8_t> encode_image(image_format format,
                                           const rgba *pixels,
                                           dim width,
                                           dim height,
                                           int jpeg_quality);
} // namespace native::detail
