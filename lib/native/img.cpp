//
// Implements owned RGBA image storage and access.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>

#include <native/graphics.h>

#include "gpx_img.h"
#include "image_codec.h"

namespace
{
    native::image_format format_from_path(const std::string &path) {
        const std::size_t dot = path.find_last_of('.');
        std::string extension = dot == std::string::npos
            ? std::string()
            : path.substr(dot);
        std::transform(
            extension.begin(),
            extension.end(),
            extension.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (extension == ".png")
            return native::image_format::png;
        if (extension == ".jpg" || extension == ".jpeg")
            return native::image_format::jpeg;
        throw std::invalid_argument(
            "img::save: expected a .png, .jpg, or .jpeg path");
    }
}

namespace native
{
    gpx_img::~gpx_img() = default;

    img::img(dim w, dim h)
        : _w(static_cast<coord>(w)), _h(static_cast<coord>(h)) {
        if (w == 0 || h == 0 ||
            w > static_cast<dim>(std::numeric_limits<coord>::max()) ||
            h > static_cast<dim>(std::numeric_limits<coord>::max())) {
            throw std::invalid_argument(
                "img: dimensions must fit positive Native coordinates");
        }
        _data = std::make_unique<rgba[]>(
            static_cast<std::size_t>(w) * h);
    }

    img::img(dim w, dim h, std::unique_ptr<rgba[]> pixels)
        : _w(static_cast<coord>(w)),
          _h(static_cast<coord>(h)),
          _data(std::move(pixels)) {
        if (w == 0 || h == 0 ||
            w > static_cast<dim>(std::numeric_limits<coord>::max()) ||
            h > static_cast<dim>(std::numeric_limits<coord>::max()) ||
            !_data) {
            throw std::invalid_argument(
                "img: decoded dimensions or pixel storage are invalid");
        }
    }

    img::~img() = default;

    img img::load(const std::string &path) {
        std::ifstream stream(path, std::ios::binary);
        if (!stream)
            throw std::runtime_error("img::load: unable to open " + path);

        auto encoded = std::vector<std::uint8_t>(
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>());
        if (stream.bad())
            throw std::runtime_error("img::load: unable to read " + path);
        return decode(encoded.data(), encoded.size());
    }

    img img::decode(const std::uint8_t *data, std::size_t size) {
        if (!data || size == 0)
            throw std::invalid_argument("img::decode: encoded data is empty");
        detail::decoded_image decoded = detail::decode_image(data, size);
        return img(
            decoded.width,
            decoded.height,
            std::move(decoded.pixels));
    }

    std::vector<std::uint8_t> img::encode(
        image_format format,
        int jpeg_quality) const {
        if (format != image_format::png && format != image_format::jpeg)
            throw std::invalid_argument(
                "img::encode: unsupported image format");
        if (format == image_format::jpeg &&
            (jpeg_quality < 1 || jpeg_quality > 100)) {
            throw std::invalid_argument(
                "img::encode: JPEG quality must be between 1 and 100");
        }
        return detail::encode_image(
            format,
            _data.get(),
            static_cast<dim>(_w),
            static_cast<dim>(_h),
            jpeg_quality);
    }

    void img::save(const std::string &path, int jpeg_quality) const {
        const auto encoded = encode(format_from_path(path), jpeg_quality);
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
            throw std::runtime_error("img::save: unable to open " + path);
        stream.write(
            reinterpret_cast<const char *>(encoded.data()),
            static_cast<std::streamsize>(encoded.size()));
        if (!stream)
            throw std::runtime_error("img::save: unable to write " + path);
    }

    coord img::w() const {
        return _w;
    }

    coord img::h() const {
        return _h;
    }

    rgba *img::pixels() {
        return _data.get();
    }

    const rgba *img::pixels() const {
        return _data.get();
    }

} // namespace native
