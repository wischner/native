//
// Implements PNG and JPEG image I/O with the Haiku Translation Kit.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <Bitmap.h>
#include <BitmapStream.h>
#include <DataIO.h>
#include <Message.h>
#include <TranslatorFormats.h>
#include <TranslatorRoster.h>

#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#include "../../image_codec.h"

namespace
{
    bool is_supported_input(const std::uint8_t *data,
                            std::size_t size) {
        const bool png =
            size >= 8 && std::memcmp(data, "\x89PNG\r\n\x1a\n", 8) == 0;
        const bool jpeg =
            size >= 2 && data[0] == 0xff && data[1] == 0xd8;
        return png || jpeg;
    }

    std::uint32_t read_big_endian(const std::uint8_t *bytes) {
        return static_cast<std::uint32_t>(bytes[0]) << 24U |
               static_cast<std::uint32_t>(bytes[1]) << 16U |
               static_cast<std::uint32_t>(bytes[2]) << 8U |
               static_cast<std::uint32_t>(bytes[3]);
    }

    float read_big_endian_float(const std::uint8_t *bytes) {
        const std::uint32_t bits = read_big_endian(bytes);
        float value = 0.0f;
        static_assert(sizeof(value) == sizeof(bits));
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    native::rgba read_pixel(const std::uint8_t *pixel,
                            color_space space) {
        if (space == B_RGBA32)
            return native::rgba(
                pixel[2], pixel[1], pixel[0], pixel[3]);
        if (space == B_RGB32)
            return native::rgba(pixel[2], pixel[1], pixel[0], 255);
        if (space == B_RGBA32_BIG)
            return native::rgba(
                pixel[1], pixel[2], pixel[3], pixel[0]);
        if (space == B_RGB32_BIG)
            return native::rgba(pixel[1], pixel[2], pixel[3], 255);
        if (space == B_RGB24)
            return native::rgba(pixel[2], pixel[1], pixel[0], 255);
        return native::rgba(pixel[0], pixel[1], pixel[2], 255);
    }

    std::size_t pixel_size(color_space space) {
        if (space == B_RGB24 || space == B_RGB24_BIG)
            return 3;
        if (space == B_RGB32 || space == B_RGBA32 ||
            space == B_RGB32_BIG || space == B_RGBA32_BIG) {
            return 4;
        }
        return 0;
    }

    void copy_to_bgra(BBitmap &bitmap,
                      const native::rgba *pixels,
                      native::dim width,
                      native::dim height) {
        auto *base = static_cast<std::uint8_t *>(bitmap.Bits());
        for (std::size_t y = 0; y < height; ++y) {
            auto *row = base + y * bitmap.BytesPerRow();
            for (std::size_t x = 0; x < width; ++x) {
                const native::rgba &pixel = pixels[y * width + x];
                row[x * 4] = pixel.b;
                row[x * 4 + 1] = pixel.g;
                row[x * 4 + 2] = pixel.r;
                row[x * 4 + 3] = pixel.a;
            }
        }
    }
} // namespace

namespace native::detail
{
    decoded_image decode_image(const std::uint8_t *data,
                               std::size_t size) {
        if (!is_supported_input(data, size))
            throw std::runtime_error(
                "Haiku image codec: input is not PNG or JPEG");

        BMallocIO input;
        if (input.Write(data, size) != static_cast<ssize_t>(size) ||
            input.Seek(0, SEEK_SET) < 0) {
            throw std::runtime_error(
                "Haiku image codec: unable to create input stream");
        }

        BMallocIO output;
        BMessage settings;
        const status_t status = BTranslatorRoster::Default()->Translate(
            &input,
            nullptr,
            &settings,
            &output,
            B_TRANSLATOR_BITMAP);
        const std::size_t output_size = output.BufferLength() > 0
                                            ? static_cast<std::size_t>(
                                                  output.BufferLength())
                                            : 0;
        if (status != B_OK || output_size < sizeof(TranslatorBitmap)) {
            throw std::runtime_error(
                "Haiku image codec: decode failed");
        }

        const auto *encoded = static_cast<const std::uint8_t *>(
            output.Buffer());
        if (read_big_endian(encoded) != B_TRANSLATOR_BITMAP)
            throw std::runtime_error(
                "Haiku image codec: invalid translated bitmap");
        const float left = read_big_endian_float(encoded + 4);
        const float top = read_big_endian_float(encoded + 8);
        const float right = read_big_endian_float(encoded + 12);
        const float bottom = read_big_endian_float(encoded + 16);
        const auto row_bytes = static_cast<std::size_t>(
            read_big_endian(encoded + 20));
        const auto space = static_cast<color_space>(
            read_big_endian(encoded + 24));
        const auto data_size = static_cast<std::size_t>(
            read_big_endian(encoded + 28));
        const std::size_t width = static_cast<std::size_t>(
            std::lround(right - left + 1.0f));
        const std::size_t height = static_cast<std::size_t>(
            std::lround(bottom - top + 1.0f));
        const std::size_t bytes_per_pixel = pixel_size(space);
        if (width == 0 || height == 0 ||
            width > static_cast<std::size_t>(
                        std::numeric_limits<coord>::max()) ||
            height > static_cast<std::size_t>(
                         std::numeric_limits<coord>::max()) ||
            bytes_per_pixel == 0 ||
            row_bytes < width * bytes_per_pixel ||
            data_size < row_bytes * height ||
            sizeof(TranslatorBitmap) + data_size >
                output_size) {
            throw std::runtime_error("Haiku image codec: dimensions "
                                     "or pixel format is invalid");
        }

        decoded_image decoded;
        decoded.width = static_cast<dim>(width);
        decoded.height = static_cast<dim>(height);
        decoded.pixels = std::make_unique<rgba[]>(width * height);
        const auto *base = encoded + sizeof(TranslatorBitmap);
        for (std::size_t y = 0; y < height; ++y) {
            const auto *row = base + y * row_bytes;
            for (std::size_t x = 0; x < width; ++x) {
                decoded.pixels[y * width + x] = read_pixel(
                    row + x * bytes_per_pixel, space);
            }
        }
        return decoded;
    }

    std::vector<std::uint8_t> encode_image(image_format format,
                                           const rgba *pixels,
                                           dim width,
                                           dim height,
                                           int jpeg_quality) {
        auto *bitmap = new BBitmap(BRect(0, 0, width - 1, height - 1),
                                   B_BITMAP_NO_SERVER_LINK,
                                   B_RGBA32);
        if (bitmap->InitCheck() != B_OK) {
            delete bitmap;
            throw std::runtime_error(
                "Haiku image codec: unable to create image");
        }
        copy_to_bgra(*bitmap, pixels, width, height);

        // BBitmapStream assumes ownership of its BBitmap.
        BBitmapStream source(bitmap);
        BMallocIO output;
        BMessage settings;
        settings.AddInt32("quality", jpeg_quality);
        const uint32 output_type =
            format == image_format::png ? B_PNG_FORMAT : B_JPEG_FORMAT;
        const status_t status = BTranslatorRoster::Default()->Translate(
            &source, nullptr, &settings, &output, output_type);
        if (status != B_OK)
            throw std::runtime_error(
                "Haiku image codec: encode failed");

        const auto *bytes =
            static_cast<const std::uint8_t *>(output.Buffer());
        return std::vector<std::uint8_t>(bytes,
                                         bytes + output.BufferLength());
    }
} // namespace native::detail
