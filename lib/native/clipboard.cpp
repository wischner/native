//
// Implements typed clipboard snapshots, staged writes, validation, and
// lossless image conversion shared by every backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/clipboard.h>

#include <algorithm>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <utility>

#include "clipboard_backend.h"
#include "text_util.h"

namespace native
{
    namespace
    {
        // Convert platform line endings in copied external text.
        std::string portable_lines(const std::string &text) {
            std::string result;
            result.reserve(text.size());
            for (std::size_t index = 0; index < text.size(); ++index) {
                if (text[index] == '\r') {
                    if (index + 1 < text.size() &&
                        text[index + 1] == '\n') {
                        ++index;
                    }
                    result.push_back('\n');
                } else {
                    result.push_back(text[index]);
                }
            }
            return result;
        }
    } // namespace

    clipboard::clipboard(clipboard_access access)
        : _access(access) {}

    clipboard::~clipboard() = default;

    clipboard::clipboard(clipboard &&other) noexcept = default;

    clipboard &clipboard::operator=(clipboard &&other) noexcept =
        default;

    clipboard clipboard::open_read() {
        clipboard stream(clipboard_access::read);
        detail::clipboard_payload payload =
            detail::read_clipboard();
        if (payload.has_text)
            payload.text = portable_lines(payload.text);
        if (payload.has_text &&
            (payload.text.find('\0') != std::string::npos ||
             !detail::valid_utf8(payload.text))) {
            throw std::runtime_error(
                "clipboard contains malformed portable text");
        }
        if (payload.has_image) {
            if (payload.image.empty())
                throw std::runtime_error(
                    "clipboard contains an empty image");
            img::decode(payload.image.data(), payload.image.size());
        }
        stream._has_text = payload.has_text;
        stream._has_image = payload.has_image;
        stream._text = std::move(payload.text);
        stream._image = std::move(payload.image);
        return stream;
    }

    clipboard clipboard::open_write() {
        return clipboard(clipboard_access::write);
    }

    clipboard_access clipboard::get_access() const {
        return _access;
    }

    std::vector<clipboard_format> clipboard::formats() const {
        std::vector<clipboard_format> result;
        if (_has_text)
            result.push_back(clipboard_format::text);
        if (_has_image)
            result.push_back(clipboard_format::image);
        return result;
    }

    bool clipboard::has(clipboard_format format) const {
        return format == clipboard_format::text ? _has_text
                                                 : _has_image;
    }

    std::size_t clipboard::size(clipboard_format format) const {
        if (!has(format))
            return 0;
        return format == clipboard_format::text ? _text.size()
                                                 : _image.size();
    }

    std::size_t clipboard::read(clipboard_format format,
                                std::size_t offset,
                                std::uint8_t *data,
                                std::size_t capacity) const {
        if (_access != clipboard_access::read)
            throw std::logic_error(
                "clipboard::read requires a read stream");
        if (!has(format) || offset >= size(format) || capacity == 0)
            return 0;
        if (!data)
            throw std::invalid_argument(
                "clipboard::read requires a destination");

        const auto *source = format == clipboard_format::text
                                 ? reinterpret_cast<
                                       const std::uint8_t *>(
                                       _text.data())
                                 : _image.data();
        const std::size_t count =
            std::min(capacity, size(format) - offset);
        std::memcpy(data, source + offset, count);
        return count;
    }

    clipboard &clipboard::write(clipboard_format format,
                                const std::uint8_t *data,
                                std::size_t count) {
        if (_access != clipboard_access::write || _committed)
            throw std::logic_error(
                "clipboard::write requires an open write stream");
        if (!data && count != 0)
            throw std::invalid_argument(
                "clipboard::write received a null source");

        if (format == clipboard_format::text) {
            const std::string text = count == 0
                                         ? std::string()
                                         : std::string(
                                               reinterpret_cast<
                                                   const char *>(data),
                                               count);
            return write_text(text);
        }

        if (count == 0)
            throw std::invalid_argument(
                "clipboard image data is empty");
        img decoded = img::decode(data, count);
        _image = decoded.encode(image_format::png);
        _has_image = true;
        return *this;
    }

    std::string clipboard::read_text() const {
        if (_access != clipboard_access::read || !_has_text)
            throw std::runtime_error(
                "clipboard has no text representation");
        return _text;
    }

    img clipboard::read_image() const {
        if (_access != clipboard_access::read || !_has_image)
            throw std::runtime_error(
                "clipboard has no image representation");
        return img::decode(_image.data(), _image.size());
    }

    clipboard &clipboard::write_text(const std::string &text) {
        if (_access != clipboard_access::write || _committed)
            throw std::logic_error(
                "clipboard::write_text requires an open write stream");
        if (text.find('\0') != std::string::npos ||
            text.find('\r') != std::string::npos ||
            !detail::valid_utf8(text)) {
            throw std::invalid_argument(
                "clipboard text must be valid null-free UTF-8 with LF "
                "line endings");
        }
        _text = text;
        _has_text = true;
        return *this;
    }

    clipboard &clipboard::write_image(const img &image) {
        if (_access != clipboard_access::write || _committed)
            throw std::logic_error(
                "clipboard::write_image requires an open write stream");
        _image = image.encode(image_format::png);
        _has_image = true;
        return *this;
    }

    void clipboard::commit() {
        if (_access != clipboard_access::write || _committed)
            throw std::logic_error(
                "clipboard::commit requires an open write stream");
        if (!_has_text && !_has_image)
            throw std::logic_error(
                "clipboard::commit requires at least one format");

        detail::clipboard_payload payload;
        payload.has_text = _has_text;
        payload.has_image = _has_image;
        payload.text = _text;
        payload.image = _image;
        const detail::clipboard_payload previous =
            detail::read_clipboard();
        try {
            detail::write_clipboard(payload);
        } catch (...) {
            const std::exception_ptr failure =
                std::current_exception();
            try {
                detail::write_clipboard(previous);
            } catch (...) {
                // Preserve the original publication failure. Backends
                // also retain or restore their native state locally.
            }
            std::rethrow_exception(failure);
        }
        _committed = true;
    }

    bool clipboard::get_committed() const {
        return _committed;
    }
} // namespace native
