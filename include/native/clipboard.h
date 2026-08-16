//
// Declares the portable typed stream used to read and atomically write
// text and image representations on the system clipboard.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "graphics.h"

namespace native
{
    // Identifies one portable clipboard representation.
    enum class clipboard_format
    {
        text,
        image
    };

    // Identifies whether a clipboard stream reads or stages a write.
    enum class clipboard_access
    {
        read,
        write
    };

    // Provides a typed snapshot or atomic write transaction.
    class clipboard
    {
    public:
        // Open a consistent snapshot of the system clipboard.
        static clipboard open_read();

        // Open an empty write transaction without changing the system.
        static clipboard open_write();

        // Clipboard streams own only portable snapshot data.
        ~clipboard();

        // Clipboard streams cannot be copied.
        clipboard(const clipboard &) = delete;

        // Clipboard streams cannot be copy-assigned.
        clipboard &operator=(const clipboard &) = delete;

        // Move a clipboard stream and its pending transaction.
        clipboard(clipboard &&other) noexcept;

        // Move-assign a clipboard stream and its pending transaction.
        clipboard &operator=(clipboard &&other) noexcept;

        // Return whether this stream reads or stages a write.
        clipboard_access get_access() const;

        // Return every available portable representation.
        std::vector<clipboard_format> formats() const;

        // Return whether a portable representation is available.
        bool has(clipboard_format format) const;

        // Return the encoded byte count for one representation.
        std::size_t size(clipboard_format format) const;

        // Copy a bounded byte range from a represented format.
        std::size_t read(clipboard_format format,
                         std::size_t offset,
                         std::uint8_t *data,
                         std::size_t capacity) const;

        // Replace a staged format from complete encoded bytes.
        clipboard &write(clipboard_format format,
                         const std::uint8_t *data,
                         std::size_t size);

        // Return the UTF-8 text representation.
        std::string read_text() const;

        // Decode and return the image representation.
        img read_image() const;

        // Stage valid UTF-8 text for the next commit.
        clipboard &write_text(const std::string &text);

        // Stage a lossless PNG representation of an image.
        clipboard &write_image(const img &image);

        // Publish every staged representation atomically.
        void commit();

        // Return whether this write stream has been committed.
        bool get_committed() const;

    private:
        // Construct an empty stream with one immutable access mode.
        explicit clipboard(clipboard_access access);

        clipboard_access _access;
        bool _committed = false;
        bool _has_text = false;
        bool _has_image = false;
        std::string _text;
        std::vector<std::uint8_t> _image;
    };
} // namespace native
