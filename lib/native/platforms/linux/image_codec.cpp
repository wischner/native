//
// Implements PNG and JPEG image I/O for Linux toolkits.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <climits>
#include <csetjmp>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <jpeglib.h>
#include <png.h>

#include "../../image_codec.h"

namespace
{
    struct jpeg_error_state
    {
        jpeg_error_mgr base;
        std::jmp_buf jump;
        char message[JMSG_LENGTH_MAX] = {};
    };

    [[noreturn]] void jpeg_error_exit(j_common_ptr codec) {
        auto *state = reinterpret_cast<jpeg_error_state *>(codec->err);
        codec->err->format_message(codec, state->message);
        std::longjmp(state->jump, 1);
    }

    void validate_dimensions(std::size_t width, std::size_t height) {
        if (width == 0 || height == 0 ||
            width > static_cast<std::size_t>(
                        std::numeric_limits<native::coord>::max()) ||
            height > static_cast<std::size_t>(
                         std::numeric_limits<native::coord>::max())) {
            throw std::runtime_error(
                "image codec: dimensions exceed native image limits");
        }
    }

    native::detail::decoded_image decode_png(const std::uint8_t *data,
                                             std::size_t size) {
        png_image image = {};
        image.version = PNG_IMAGE_VERSION;
        if (!png_image_begin_read_from_memory(&image, data, size))
            throw std::runtime_error(
                std::string("PNG decode failed: ") + image.message);

        try {
            validate_dimensions(image.width, image.height);
            image.format = PNG_FORMAT_RGBA;

            native::detail::decoded_image decoded;
            decoded.width = static_cast<native::dim>(image.width);
            decoded.height = static_cast<native::dim>(image.height);
            decoded.pixels = std::make_unique<native::rgba[]>(
                static_cast<std::size_t>(decoded.width) *
                decoded.height);

            if (!png_image_finish_read(&image,
                                       nullptr,
                                       decoded.pixels.get(),
                                       0,
                                       nullptr)) {
                throw std::runtime_error(
                    std::string("PNG decode failed: ") + image.message);
            }
            png_image_free(&image);
            return decoded;
        } catch (...) {
            png_image_free(&image);
            throw;
        }
    }

    native::detail::decoded_image decode_jpeg(const std::uint8_t *data,
                                              std::size_t size) {
        if (size > ULONG_MAX)
            throw std::runtime_error(
                "JPEG decode failed: input is too large");

        jpeg_decompress_struct codec = {};
        jpeg_error_state error = {};
        native::detail::decoded_image decoded;
        std::vector<std::uint8_t> row;

        codec.err = jpeg_std_error(&error.base);
        error.base.error_exit = jpeg_error_exit;
        if (setjmp(error.jump)) {
            jpeg_destroy_decompress(&codec);
            throw std::runtime_error(
                std::string("JPEG decode failed: ") + error.message);
        }

        jpeg_create_decompress(&codec);
        try {
            jpeg_mem_src(&codec,
                         const_cast<unsigned char *>(data),
                         static_cast<unsigned long>(size));
            jpeg_read_header(&codec, TRUE);
            codec.out_color_space = JCS_RGB;
            jpeg_start_decompress(&codec);

            validate_dimensions(codec.output_width,
                                codec.output_height);
            if (codec.output_components != 3)
                throw std::runtime_error(
                    "JPEG decode failed: unsupported output color "
                    "space");

            decoded.width =
                static_cast<native::dim>(codec.output_width);
            decoded.height =
                static_cast<native::dim>(codec.output_height);
            decoded.pixels = std::make_unique<native::rgba[]>(
                static_cast<std::size_t>(decoded.width) *
                decoded.height);
            row.resize(static_cast<std::size_t>(decoded.width) * 3);

            while (codec.output_scanline < codec.output_height) {
                JSAMPROW rows[] = {row.data()};
                const std::size_t y = codec.output_scanline;
                jpeg_read_scanlines(&codec, rows, 1);
                for (std::size_t x = 0; x < decoded.width; ++x) {
                    decoded.pixels[y * decoded.width + x] =
                        native::rgba(row[x * 3],
                                     row[x * 3 + 1],
                                     row[x * 3 + 2],
                                     255);
                }
            }

            jpeg_finish_decompress(&codec);
            jpeg_destroy_decompress(&codec);
            return decoded;
        } catch (...) {
            jpeg_destroy_decompress(&codec);
            throw;
        }
    }

    std::vector<std::uint8_t> encode_png(const native::rgba *pixels,
                                         native::dim width,
                                         native::dim height) {
        png_image image = {};
        image.version = PNG_IMAGE_VERSION;
        image.width = width;
        image.height = height;
        image.format = PNG_FORMAT_RGBA;

        png_alloc_size_t encoded_size = 0;
        if (!png_image_write_to_memory(&image,
                                       nullptr,
                                       &encoded_size,
                                       0,
                                       pixels,
                                       0,
                                       nullptr)) {
            throw std::runtime_error(
                std::string("PNG encode failed: ") + image.message);
        }

        std::vector<std::uint8_t> encoded(encoded_size);
        if (!png_image_write_to_memory(&image,
                                       encoded.data(),
                                       &encoded_size,
                                       0,
                                       pixels,
                                       0,
                                       nullptr)) {
            throw std::runtime_error(
                std::string("PNG encode failed: ") + image.message);
        }
        encoded.resize(encoded_size);
        return encoded;
    }

    std::vector<std::uint8_t> encode_jpeg(const native::rgba *pixels,
                                          native::dim width,
                                          native::dim height,
                                          int quality) {
        jpeg_compress_struct codec = {};
        jpeg_error_state error = {};
        std::vector<std::uint8_t> encoded;
        std::vector<std::uint8_t> row(static_cast<std::size_t>(width) *
                                      3);
        unsigned char *output = nullptr;
        unsigned long output_size = 0;

        codec.err = jpeg_std_error(&error.base);
        error.base.error_exit = jpeg_error_exit;
        if (setjmp(error.jump)) {
            jpeg_destroy_compress(&codec);
            std::free(output);
            throw std::runtime_error(
                std::string("JPEG encode failed: ") + error.message);
        }

        jpeg_create_compress(&codec);
        jpeg_mem_dest(&codec, &output, &output_size);
        codec.image_width = width;
        codec.image_height = height;
        codec.input_components = 3;
        codec.in_color_space = JCS_RGB;
        jpeg_set_defaults(&codec);
        jpeg_set_quality(&codec, quality, TRUE);
        jpeg_start_compress(&codec, TRUE);

        while (codec.next_scanline < codec.image_height) {
            const native::rgba *source =
                pixels +
                static_cast<std::size_t>(codec.next_scanline) * width;
            for (std::size_t x = 0; x < width; ++x) {
                row[x * 3] = source[x].r;
                row[x * 3 + 1] = source[x].g;
                row[x * 3 + 2] = source[x].b;
            }
            JSAMPROW rows[] = {row.data()};
            jpeg_write_scanlines(&codec, rows, 1);
        }

        jpeg_finish_compress(&codec);
        encoded.assign(output, output + output_size);
        jpeg_destroy_compress(&codec);
        std::free(output);
        return encoded;
    }
} // namespace

namespace native::detail
{
    decoded_image decode_image(const std::uint8_t *data,
                               std::size_t size) {
        if (size >= 8 && png_sig_cmp(data, 0, 8) == 0)
            return decode_png(data, size);
        if (size >= 2 && data[0] == 0xff && data[1] == 0xd8)
            return decode_jpeg(data, size);
        throw std::runtime_error(
            "image codec: input is not PNG or JPEG");
    }

    std::vector<std::uint8_t> encode_image(image_format format,
                                           const rgba *pixels,
                                           dim width,
                                           dim height,
                                           int jpeg_quality) {
        switch (format) {
        case image_format::png:
            return encode_png(pixels, width, height);
        case image_format::jpeg:
            return encode_jpeg(pixels, width, height, jpeg_quality);
        }
        throw std::invalid_argument(
            "image codec: unsupported image format");
    }
} // namespace native::detail
