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
#include <TranslationUtils.h>
#include <TranslatorFormats.h>
#include <TranslatorRoster.h>

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

        std::unique_ptr<BBitmap> source(
            BTranslationUtils::GetBitmap(&input));
        if (!source || source->InitCheck() != B_OK)
            throw std::runtime_error(
                "Haiku image codec: decode failed");

        const std::size_t width =
            static_cast<std::size_t>(source->Bounds().IntegerWidth()) +
            1;
        const std::size_t height =
            static_cast<std::size_t>(source->Bounds().IntegerHeight()) +
            1;
        if (width == 0 || height == 0 ||
            width > static_cast<std::size_t>(
                        std::numeric_limits<coord>::max()) ||
            height > static_cast<std::size_t>(
                         std::numeric_limits<coord>::max())) {
            throw std::runtime_error("Haiku image codec: dimensions "
                                     "exceed native image limits");
        }

        BBitmap rgba_bitmap(
            source->Bounds(), B_BITMAP_NO_SERVER_LINK, B_RGBA32);
        if (rgba_bitmap.InitCheck() != B_OK ||
            rgba_bitmap.ImportBits(source.get()) != B_OK) {
            throw std::runtime_error(
                "Haiku image codec: unable to convert decoded pixels");
        }

        decoded_image decoded;
        decoded.width = static_cast<dim>(width);
        decoded.height = static_cast<dim>(height);
        decoded.pixels = std::make_unique<rgba[]>(width * height);
        const auto *base =
            static_cast<const std::uint8_t *>(rgba_bitmap.Bits());
        for (std::size_t y = 0; y < height; ++y) {
            const auto *row = base + y * rgba_bitmap.BytesPerRow();
            for (std::size_t x = 0; x < width; ++x) {
                decoded.pixels[y * width + x] = rgba(row[x * 4 + 2],
                                                     row[x * 4 + 1],
                                                     row[x * 4],
                                                     row[x * 4 + 3]);
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
