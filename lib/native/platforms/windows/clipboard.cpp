//
// Implements Windows clipboard snapshots and atomic publication for
// Unicode text, PNG, and interoperable 32-bit DIB image data.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "../../clipboard_backend.h"

#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

#include <windows.h>

#include <native.h>

#include "globals.h"

namespace
{
    // Convert portable line feeds to the CF_UNICODETEXT convention.
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

    // Keep the system clipboard open for one bounded operation.
    class clipboard_lock
    {
    public:
        clipboard_lock() {
            HWND owner = native::app::main_wnd()
                             ? windows::wnd_bindings.handle_from_object(
                                   native::app::main_wnd())
                             : nullptr;
            if (!OpenClipboard(owner))
                throw std::runtime_error(
                    "Windows: Unable to open the clipboard.");
        }

        clipboard_lock(const clipboard_lock &) = delete;
        clipboard_lock &operator=(const clipboard_lock &) = delete;

        ~clipboard_lock() {
            CloseClipboard();
        }
    };

    // Copy bytes into movable global storage accepted by SetClipboardData.
    HGLOBAL global_bytes(const void *data, std::size_t size) {
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
        if (!memory)
            throw std::bad_alloc();
        void *target = GlobalLock(memory);
        if (!target) {
            GlobalFree(memory);
            throw std::runtime_error(
                "Windows: Unable to lock clipboard memory.");
        }
        std::memcpy(target, data, size);
        GlobalUnlock(memory);
        return memory;
    }

    // Return a complete copied block from one clipboard format.
    std::vector<std::uint8_t> read_bytes(UINT format) {
        HANDLE handle = GetClipboardData(format);
        if (!handle)
            return {};
        const SIZE_T size = GlobalSize(handle);
        const void *data = GlobalLock(handle);
        if (!data || size == 0) {
            if (data)
                GlobalUnlock(handle);
            return {};
        }
        const auto *first = static_cast<const std::uint8_t *>(data);
        std::vector<std::uint8_t> result(first, first + size);
        GlobalUnlock(handle);
        return result;
    }

    // Encode Native RGBA pixels as a top-down BITMAPV5 clipboard block.
    HGLOBAL dib_from_png(const std::vector<std::uint8_t> &png) {
        native::img image = native::img::decode(png.data(), png.size());
        const std::size_t pixel_count =
            static_cast<std::size_t>(image.w()) * image.h();
        const std::size_t size =
            sizeof(BITMAPV5HEADER) + pixel_count * 4;
        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
        if (!memory)
            throw std::bad_alloc();

        auto *header = static_cast<BITMAPV5HEADER *>(GlobalLock(memory));
        if (!header) {
            GlobalFree(memory);
            throw std::runtime_error(
                "Windows: Unable to lock DIB clipboard memory.");
        }
        std::memset(header, 0, sizeof(*header));
        header->bV5Size = sizeof(*header);
        header->bV5Width = image.w();
        header->bV5Height = -image.h();
        header->bV5Planes = 1;
        header->bV5BitCount = 32;
        header->bV5Compression = BI_BITFIELDS;
        header->bV5SizeImage =
            static_cast<DWORD>(pixel_count * 4);
        header->bV5RedMask = 0x00ff0000;
        header->bV5GreenMask = 0x0000ff00;
        header->bV5BlueMask = 0x000000ff;
        header->bV5AlphaMask = 0xff000000;
        header->bV5CSType = 0x73524742;

        auto *pixels = reinterpret_cast<std::uint8_t *>(header + 1);
        for (std::size_t index = 0; index < pixel_count; ++index) {
            const native::rgba source = image.pixels()[index];
            pixels[index * 4] = source.b;
            pixels[index * 4 + 1] = source.g;
            pixels[index * 4 + 2] = source.r;
            pixels[index * 4 + 3] = source.a;
        }
        GlobalUnlock(memory);
        return memory;
    }

    // Decode a 32-bit DIB clipboard block into a lossless PNG.
    std::vector<std::uint8_t> png_from_dib() {
        HANDLE handle = GetClipboardData(CF_DIBV5);
        if (!handle)
            handle = GetClipboardData(CF_DIB);
        if (!handle)
            return {};

        const auto *header = static_cast<const BITMAPINFOHEADER *>(
            GlobalLock(handle));
        const SIZE_T block_size = GlobalSize(handle);
        if (!header || block_size < sizeof(BITMAPINFOHEADER) ||
            header->biSize < sizeof(BITMAPINFOHEADER) ||
            header->biWidth <= 0 || header->biHeight == 0 ||
            header->biHeight == std::numeric_limits<LONG>::min() ||
            header->biPlanes != 1 || header->biBitCount != 32 ||
            (header->biCompression != BI_RGB &&
             header->biCompression != BI_BITFIELDS)) {
            if (header)
                GlobalUnlock(handle);
            return {};
        }

        const int width = header->biWidth;
        const int height =
            header->biHeight < 0 ? -header->biHeight : header->biHeight;
        if (width > std::numeric_limits<native::coord>::max() ||
            height > std::numeric_limits<native::coord>::max()) {
            GlobalUnlock(handle);
            return {};
        }
        const std::size_t pixel_bytes =
            static_cast<std::size_t>(width) * height * 4;
        std::size_t offset = header->biSize;
        if (header->biSize == sizeof(BITMAPINFOHEADER) &&
            header->biCompression == BI_BITFIELDS) {
            offset += 3 * sizeof(DWORD);
        }
        if (offset > block_size || pixel_bytes > block_size - offset) {
            GlobalUnlock(handle);
            return {};
        }

        native::img image(static_cast<native::dim>(width),
                          static_cast<native::dim>(height));
        const auto *source =
            reinterpret_cast<const std::uint8_t *>(header) + offset;
        const bool top_down = header->biHeight < 0;
        const bool alpha_defined =
            block_size >= sizeof(BITMAPV5HEADER) &&
            header->biSize >= sizeof(BITMAPV5HEADER) &&
            static_cast<const BITMAPV5HEADER *>(
                static_cast<const void *>(header))
                    ->bV5AlphaMask != 0;
        for (int y = 0; y < height; ++y) {
            const int source_y = top_down ? y : height - y - 1;
            for (int x = 0; x < width; ++x) {
                const std::size_t source_index =
                    (static_cast<std::size_t>(source_y) * width + x) *
                    4;
                image.pixels()[static_cast<std::size_t>(y) * width + x] =
                    native::rgba(source[source_index + 2],
                                 source[source_index + 1],
                                 source[source_index],
                                 alpha_defined
                                     ? source[source_index + 3]
                                     : 255);
            }
        }
        GlobalUnlock(handle);
        return image.encode(native::image_format::png);
    }
} // namespace

namespace native::detail
{
    clipboard_payload read_clipboard() {
        clipboard_lock lock;
        clipboard_payload payload;

        if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
            HANDLE handle = GetClipboardData(CF_UNICODETEXT);
            const SIZE_T bytes = handle ? GlobalSize(handle) : 0;
            const auto *text =
                handle && bytes >= sizeof(wchar_t)
                    ? static_cast<const wchar_t *>(GlobalLock(handle))
                    : nullptr;
            if (text) {
                const std::size_t capacity = bytes / sizeof(wchar_t);
                std::size_t length = 0;
                while (length < capacity && text[length] != L'\0')
                    ++length;
                const bool terminated = length < capacity;
                const std::wstring copied =
                    terminated ? std::wstring(text, length)
                               : std::wstring();
                GlobalUnlock(handle);
                if (!terminated) {
                    throw std::runtime_error(
                        "Windows: Malformed clipboard Unicode text.");
                }
                payload.text = windows::wide_to_utf8(copied);
                payload.has_text = true;
            }
        }

        const UINT png_format = RegisterClipboardFormatW(L"PNG");
        if (png_format && IsClipboardFormatAvailable(png_format))
            payload.image = read_bytes(png_format);
        if (payload.image.empty())
            payload.image = png_from_dib();
        payload.has_image = !payload.image.empty();
        return payload;
    }

    void write_clipboard(const clipboard_payload &payload) {
        std::wstring wide;
        HGLOBAL text_memory = nullptr;
        HGLOBAL png_memory = nullptr;
        HGLOBAL dib_memory = nullptr;
        const UINT png_format = RegisterClipboardFormatW(L"PNG");

        try {
            if (payload.has_text) {
                wide = windows::utf8_to_wide(
                    native_lines(payload.text));
                text_memory = global_bytes(
                    wide.c_str(), (wide.size() + 1) * sizeof(wchar_t));
            }
            if (payload.has_image) {
                png_memory = global_bytes(payload.image.data(),
                                          payload.image.size());
                dib_memory = dib_from_png(payload.image);
            }

            clipboard_lock lock;
            if (!EmptyClipboard())
                throw std::runtime_error(
                    "Windows: Unable to clear the clipboard.");
            if (text_memory &&
                !SetClipboardData(CF_UNICODETEXT, text_memory)) {
                throw std::runtime_error(
                    "Windows: Unable to publish clipboard text.");
            }
            if (text_memory)
                text_memory = nullptr;
            if (png_memory &&
                (!png_format ||
                 !SetClipboardData(png_format, png_memory))) {
                throw std::runtime_error(
                    "Windows: Unable to publish clipboard PNG.");
            }
            if (png_memory)
                png_memory = nullptr;
            if (dib_memory && !SetClipboardData(CF_DIBV5, dib_memory))
                throw std::runtime_error(
                    "Windows: Unable to publish clipboard bitmap.");
            if (dib_memory)
                dib_memory = nullptr;
        } catch (...) {
            if (text_memory)
                GlobalFree(text_memory);
            if (png_memory)
                GlobalFree(png_memory);
            if (dib_memory)
                GlobalFree(dib_memory);
            throw;
        }
    }
} // namespace native::detail
