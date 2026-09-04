//
// Implements GEM AES scrap clipboard snapshots and staged text/PNG/IMG
// publication through conventional SCRAP files.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "../../clipboard_backend.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <gem.h>

#include <native.h>

namespace
{
    namespace fs = std::filesystem;

    constexpr std::size_t maximum_scrap_size = 512 * 1024 * 1024;
    constexpr std::size_t maximum_image_pixels =
        maximum_scrap_size / sizeof(native::rgba);

    std::string scrap_directory(bool create) {
        char value[1024] = {};
        if (scrp_read(value) != 0 && value[0] != '\0')
            return value;
        if (!create)
            return {};
        std::error_code error;
        std::string directory = fs::temp_directory_path(error).string();
        if (error || directory.empty()) {
            throw std::runtime_error(
                "GEMix: Unable to find temporary storage.");
        }
        if (directory.back() != fs::path::preferred_separator)
            directory.push_back(fs::path::preferred_separator);
        if (scrp_write(const_cast<char *>(directory.c_str())) == 0) {
            throw std::runtime_error(
                "GEMix: Unable to configure the AES scrap directory.");
        }
        return directory;
    }

    std::string scrap_path(const std::string &directory,
                           const char *name) {
        if (directory.empty())
            return {};
        return (fs::path(directory) / name).string();
    }

    std::vector<std::uint8_t> read_file(const std::string &path) {
        std::ifstream stream(path,
                             std::ios::binary | std::ios::ate);
        if (!stream)
            return {};
        const std::streamoff length = stream.tellg();
        if (length < 0 ||
            static_cast<std::uintmax_t>(length) >
                maximum_scrap_size) {
            throw std::runtime_error(
                "GEMix: AES scrap file is too large.");
        }
        stream.seekg(0);
        return std::vector<std::uint8_t>(
            std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>());
    }

    void write_file(const std::string &path,
                    const std::uint8_t *data,
                    std::size_t size) {
        std::ofstream stream(path,
                             std::ios::binary | std::ios::trunc);
        if (!stream ||
            (size != 0 &&
             !stream.write(reinterpret_cast<const char *>(data),
                           static_cast<std::streamsize>(size)))) {
            throw std::runtime_error(
                "GEMix: Unable to stage an AES scrap file.");
        }
    }

    std::string native_lines(const std::string &text) {
        std::string result;
        result.reserve(text.size());
        for (char value : text) {
            if (value == '\n')
                result += "\r\n";
            else
                result.push_back(value);
        }
        return result;
    }

    std::string portable_lines(const std::vector<std::uint8_t> &data) {
        std::string result;
        result.reserve(data.size());
        for (std::size_t index = 0; index < data.size(); ++index) {
            const char value = static_cast<char>(data[index]);
            if (value == '\r') {
                if (index + 1 < data.size() && data[index + 1] == '\n')
                    ++index;
                result.push_back('\n');
            } else if (value != '\0') {
                result.push_back(value);
            }
        }
        return result;
    }

    std::uint16_t read_word(const std::vector<std::uint8_t> &data,
                            std::size_t offset) {
        if (offset > data.size() || data.size() - offset < 2) {
            throw std::runtime_error(
                "GEMix: Truncated GEM IMG clipboard data.");
        }
        return static_cast<std::uint16_t>(
            static_cast<std::uint16_t>(data[offset]) << 8 |
            data[offset + 1]);
    }

    void append_word(std::vector<std::uint8_t> &data,
                     std::uint16_t value) {
        data.push_back(static_cast<std::uint8_t>(value >> 8));
        data.push_back(static_cast<std::uint8_t>(value));
    }

    std::vector<std::uint8_t> decode_img_row(
        const std::vector<std::uint8_t> &data,
        std::size_t &offset,
        std::size_t row_size,
        std::size_t pattern_size) {
        std::vector<std::uint8_t> row;
        row.reserve(row_size);
        while (row.size() < row_size) {
            if (offset >= data.size()) {
                throw std::runtime_error(
                    "GEMix: Truncated GEM IMG scan line.");
            }
            const std::uint8_t code = data[offset++];
            if (code == 0x80) {
                if (offset >= data.size()) {
                    throw std::runtime_error(
                        "GEMix: Truncated GEM IMG literal run.");
                }
                const std::size_t count = data[offset++];
                if (count == 0 || count > row_size - row.size() ||
                    offset > data.size() ||
                    count > data.size() - offset) {
                    throw std::runtime_error(
                        "GEMix: Invalid GEM IMG literal run.");
                }
                row.insert(row.end(),
                           data.begin() + offset,
                           data.begin() + offset + count);
                offset += count;
            } else if (code == 0) {
                if (offset >= data.size()) {
                    throw std::runtime_error(
                        "GEMix: Truncated GEM IMG pattern run.");
                }
                const std::size_t count = data[offset++];
                if (count == 0 || pattern_size == 0 ||
                    pattern_size > row_size - row.size() ||
                    count >
                        (row_size - row.size()) / pattern_size ||
                    offset > data.size() ||
                    pattern_size > data.size() - offset) {
                    throw std::runtime_error(
                        "GEMix: Invalid GEM IMG pattern run.");
                }
                for (std::size_t index = 0; index < count; ++index) {
                    row.insert(row.end(),
                               data.begin() + offset,
                               data.begin() + offset + pattern_size);
                }
                offset += pattern_size;
            } else {
                const std::size_t count = code & 0x7f;
                if (count == 0 || count > row_size - row.size()) {
                    throw std::runtime_error(
                        "GEMix: Invalid GEM IMG solid run.");
                }
                row.insert(row.end(),
                           count,
                           code & 0x80 ? 0xff : 0x00);
            }
        }
        return row;
    }

    std::vector<std::uint8_t> png_from_gem_image(
        const std::vector<std::uint8_t> &data) {
        if (data.size() < 16 || read_word(data, 0) != 1) {
            throw std::runtime_error(
                "GEMix: Unsupported GEM IMG clipboard data.");
        }
        const std::size_t header_size = read_word(data, 2) * 2;
        const std::size_t planes = read_word(data, 4);
        const std::size_t pattern_size = read_word(data, 6);
        const std::size_t width = read_word(data, 12);
        const std::size_t height = read_word(data, 14);
        if (header_size < 16 || header_size > data.size() ||
            planes != 1 || pattern_size == 0 || pattern_size > 8 ||
            width == 0 || height == 0 ||
            width > maximum_image_pixels / height ||
            width > static_cast<std::size_t>(
                        std::numeric_limits<native::dim>::max()) ||
            height > static_cast<std::size_t>(
                         std::numeric_limits<native::dim>::max())) {
            throw std::runtime_error(
                "GEMix: Invalid GEM IMG clipboard header.");
        }

        native::img image(static_cast<native::dim>(width),
                          static_cast<native::dim>(height));
        const std::size_t row_size = (width + 7) / 8;
        std::size_t offset = header_size;
        std::size_t y = 0;
        while (y < height) {
            std::size_t repeat = 1;
            if (offset <= data.size() && data.size() - offset >= 4 &&
                data[offset] == 0 && data[offset + 1] == 0 &&
                data[offset + 2] == 0xff) {
                repeat = data[offset + 3];
                offset += 4;
                if (repeat == 0 || repeat > height - y) {
                    throw std::runtime_error(
                        "GEMix: Invalid GEM IMG vertical run.");
                }
            }
            const std::vector<std::uint8_t> row =
                decode_img_row(data,
                               offset,
                               row_size,
                               pattern_size);
            for (std::size_t copy = 0; copy < repeat; ++copy) {
                for (std::size_t x = 0; x < width; ++x) {
                    const bool black =
                        (row[x / 8] & (0x80 >> (x % 8))) != 0;
                    const std::uint8_t value = black ? 0 : 255;
                    image.pixels()[(y + copy) * width + x] =
                        native::rgba(value, value, value, 255);
                }
            }
            y += repeat;
        }
        return image.encode(native::image_format::png);
    }

    std::vector<std::uint8_t> gem_image_from_png(
        const std::vector<std::uint8_t> &png) {
        const native::img image =
            native::img::decode(png.data(), png.size());
        if (image.w() > std::numeric_limits<std::uint16_t>::max() ||
            image.h() > std::numeric_limits<std::uint16_t>::max()) {
            throw std::runtime_error(
                "GEMix: Image dimensions exceed GEM IMG limits.");
        }

        std::vector<std::uint8_t> data;
        append_word(data, 1);
        append_word(data, 8);
        append_word(data, 1);
        append_word(data, 1);
        append_word(data, 85);
        append_word(data, 85);
        append_word(data, static_cast<std::uint16_t>(image.w()));
        append_word(data, static_cast<std::uint16_t>(image.h()));

        const std::size_t row_size = (image.w() + 7) / 8;
        std::vector<std::uint8_t> row(row_size);
        for (native::coord y = 0; y < image.h(); ++y) {
            std::fill(row.begin(), row.end(), 0);
            for (native::coord x = 0; x < image.w(); ++x) {
                const native::rgba pixel =
                    image.pixels()[static_cast<std::size_t>(y) *
                                       image.w() +
                                   x];
                const unsigned luminance =
                    299u * pixel.r + 587u * pixel.g + 114u * pixel.b;
                if (pixel.a >= 128 && luminance < 128000u) {
                    row[static_cast<std::size_t>(x) / 8] |=
                        static_cast<std::uint8_t>(
                            0x80 >> (static_cast<unsigned>(x) % 8));
                }
            }
            std::size_t begin = 0;
            while (begin < row.size()) {
                const std::size_t count =
                    std::min<std::size_t>(255, row.size() - begin);
                data.push_back(0x80);
                data.push_back(static_cast<std::uint8_t>(count));
                data.insert(data.end(),
                            row.begin() + begin,
                            row.begin() + begin + count);
                begin += count;
            }
        }
        return data;
    }

    bool file_exists(const std::string &path) {
        std::error_code error;
        return fs::is_regular_file(fs::path(path), error);
    }

    struct scrap_file
    {
        std::string target;
        std::string stage;
        std::string backup;
        bool publish = false;
        bool backed_up = false;
        bool published = false;
    };

    void publish_files(std::vector<scrap_file> &files) {
        try {
            for (scrap_file &file : files) {
                std::error_code error;
                fs::remove(fs::path(file.backup), error);
                if (file_exists(file.target)) {
                    error.clear();
                    fs::rename(fs::path(file.target),
                               fs::path(file.backup),
                               error);
                    if (error) {
                        throw std::runtime_error(
                            "GEMix: Unable to back up AES scrap data.");
                    }
                    file.backed_up = true;
                }
            }
            for (scrap_file &file : files) {
                if (file.publish) {
                    std::error_code error;
                    fs::rename(fs::path(file.stage),
                               fs::path(file.target),
                               error);
                    if (error) {
                        throw std::runtime_error(
                            "GEMix: Unable to publish AES scrap data.");
                    }
                    file.published = true;
                }
            }
            for (scrap_file &file : files) {
                if (file.backed_up) {
                    std::error_code error;
                    fs::remove(fs::path(file.backup), error);
                }
            }
        } catch (...) {
            for (scrap_file &file : files) {
                if (file.published) {
                    std::error_code error;
                    fs::remove(fs::path(file.target), error);
                }
            }
            for (scrap_file &file : files) {
                std::error_code error;
                if (file.backed_up) {
                    fs::rename(fs::path(file.backup),
                               fs::path(file.target),
                               error);
                }
                error.clear();
                fs::remove(fs::path(file.stage), error);
            }
            throw;
        }
    }
} // namespace

namespace native::detail
{
    clipboard_payload read_clipboard() {
        clipboard_payload payload;
        const std::string directory = scrap_directory(false);
        if (directory.empty())
            return payload;
        const std::vector<std::uint8_t> text =
            read_file(scrap_path(directory, "SCRAP.TXT"));
        if (!text.empty() ||
            file_exists(scrap_path(directory, "SCRAP.TXT"))) {
            payload.text = portable_lines(text);
            payload.has_text = true;
        }
        payload.image = read_file(scrap_path(directory, "SCRAP.PNG"));
        if (payload.image.empty()) {
            const std::vector<std::uint8_t> gem_image =
                read_file(scrap_path(directory, "SCRAP.IMG"));
            if (!gem_image.empty())
                payload.image = png_from_gem_image(gem_image);
        }
        payload.has_image = !payload.image.empty();
        return payload;
    }

    void write_clipboard(const clipboard_payload &payload) {
        const std::string directory = scrap_directory(true);
        const std::string text_path =
            scrap_path(directory, "SCRAP.TXT");
        const std::string image_path =
            scrap_path(directory, "SCRAP.PNG");
        const std::string gem_image_path =
            scrap_path(directory, "SCRAP.IMG");
        std::vector<scrap_file> files = {
            {text_path,
             text_path + ".TMP",
             text_path + ".BAK",
             payload.has_text},
            {image_path,
             image_path + ".TMP",
             image_path + ".BAK",
             payload.has_image},
            {gem_image_path,
             gem_image_path + ".TMP",
             gem_image_path + ".BAK",
             payload.has_image}};
        try {
            if (payload.has_text) {
                const std::string text = native_lines(payload.text);
                write_file(files[0].stage,
                           reinterpret_cast<const std::uint8_t *>(
                               text.data()),
                           text.size());
            }
            if (payload.has_image) {
                write_file(files[1].stage,
                           payload.image.data(),
                           payload.image.size());
                const std::vector<std::uint8_t> gem_image =
                    gem_image_from_png(payload.image);
                write_file(files[2].stage,
                           gem_image.data(),
                           gem_image.size());
            }
            publish_files(files);
        } catch (...) {
            for (const scrap_file &file : files) {
                std::error_code error;
                fs::remove(fs::path(file.stage), error);
            }
            throw;
        }
    }
} // namespace native::detail
