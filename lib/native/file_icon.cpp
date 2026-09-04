//
// Implements exact-size PNG file icons with a portable generic fallback.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/filesystem.h>

#include <filesystem>
#include <limits>
#include <stdexcept>
#include <utility>

#include <native/graphics.h>

#include "filesystem_backend.h"
#include "generic_file_icon_data.h"

namespace
{
    const native::img &generic_image(bool directory) {
        using namespace native::detail::generic_file_icon_data;
        if (directory) {
            static const native::img image = native::img::decode(
                directory_png, sizeof(directory_png));
            return image;
        }
        static const native::img image = native::img::decode(
            file_png, sizeof(file_png));
        return image;
    }

    void draw_generic(native::img &target, bool directory) {
        target.get_gpx()
            .clear(native::rgba(0, 0, 0, 0))
            .draw_img(generic_image(directory),
                      native::rect(0, 0, target.w(), target.h()),
                      native::image_filter::linear);
    }
} // namespace

namespace native
{
    file_icon::file_icon(dim size,
                         file_icon_source source,
                         std::vector<std::uint8_t> png)
        : _size(size)
        , _source(source)
        , _png(std::move(png)) {}

    file_icon file_icon::from_path(
        const std::filesystem::path &path, dim size) {
        std::error_code error;
        const bool directory = std::filesystem::is_directory(path, error);
        return obtain(path, size, directory);
    }

    file_icon file_icon::for_file(
        const std::filesystem::path &path, dim size) {
        return obtain(path, size, false);
    }

    file_icon file_icon::for_directory(
        const std::filesystem::path &path, dim size) {
        return obtain(path, size, true);
    }

    file_icon file_icon::obtain(const std::filesystem::path &path,
                                dim size,
                                bool directory) {
        if (size == 0 ||
            size > static_cast<dim>(std::numeric_limits<coord>::max())) {
            throw std::invalid_argument(
                "file_icon: size must fit positive Native coordinates");
        }

        img image(size, size);
        bool loaded = false;
        if (!path.empty()) {
            try {
                loaded = detail::load_native_file_icon(
                    path, directory, image);
            } catch (...) {
                loaded = false;
            }
        }
        const file_icon_source source = loaded
                                            ? file_icon_source::native
                                            : directory
                                                  ? file_icon_source::
                                                        generic_directory
                                                  : file_icon_source::
                                                        generic_file;
        if (!loaded)
            draw_generic(image, directory);
        return file_icon(size,
                         source,
                         image.encode(image_format::png));
    }

    dim file_icon::get_size() const {
        return _size;
    }

    file_icon_source file_icon::get_source() const {
        return _source;
    }

    bool file_icon::is_generic() const {
        return _source != file_icon_source::native;
    }

    const std::vector<std::uint8_t> &file_icon::get_png() const {
        return _png;
    }
} // namespace native
