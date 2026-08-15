//
// Implements PNG and JPEG image I/O with the Windows GDI+ codecs.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#include "../../image_codec.h"

namespace
{
    class gdiplus_runtime
    {
    public:
        gdiplus_runtime() {
            Gdiplus::GdiplusStartupInput input;
            if (Gdiplus::GdiplusStartup(&_token, &input, nullptr) !=
                Gdiplus::Ok) {
                throw std::runtime_error(
                    "Windows image codec: GDI+ startup failed");
            }
        }

        ~gdiplus_runtime() {
            Gdiplus::GdiplusShutdown(_token);
        }

        gdiplus_runtime(const gdiplus_runtime &) = delete;
        gdiplus_runtime &operator=(const gdiplus_runtime &) = delete;

    private:
        ULONG_PTR _token = 0;
    };

    gdiplus_runtime &runtime() {
        static gdiplus_runtime instance;
        return instance;
    }

    struct stream_releaser
    {
        void operator()(IStream *stream) const {
            if (stream)
                stream->Release();
        }
    };

    using stream_ptr = std::unique_ptr<IStream, stream_releaser>;

    stream_ptr input_stream(const std::uint8_t *data, std::size_t size) {
        if (size > std::numeric_limits<SIZE_T>::max())
            throw std::runtime_error(
                "Windows image codec: input is too large");

        HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
        if (!memory)
            throw std::bad_alloc();
        void *destination = GlobalLock(memory);
        if (!destination) {
            GlobalFree(memory);
            throw std::runtime_error(
                "Windows image codec: unable to lock input memory");
        }
        std::memcpy(destination, data, size);
        GlobalUnlock(memory);

        IStream *stream = nullptr;
        if (CreateStreamOnHGlobal(memory, TRUE, &stream) != S_OK) {
            GlobalFree(memory);
            throw std::runtime_error(
                "Windows image codec: unable to create input stream");
        }
        return stream_ptr(stream);
    }

    stream_ptr output_stream() {
        IStream *stream = nullptr;
        if (CreateStreamOnHGlobal(nullptr, TRUE, &stream) != S_OK)
            throw std::runtime_error(
                "Windows image codec: unable to create output stream");
        return stream_ptr(stream);
    }

    CLSID encoder_clsid(const wchar_t *mime_type) {
        UINT count = 0;
        UINT bytes = 0;
        if (Gdiplus::GetImageEncodersSize(&count, &bytes) != Gdiplus::Ok ||
            count == 0 || bytes == 0) {
            throw std::runtime_error(
                "Windows image codec: no encoders are installed");
        }

        std::vector<std::uint8_t> storage(bytes);
        auto *codecs = reinterpret_cast<Gdiplus::ImageCodecInfo *>(
            storage.data());
        if (Gdiplus::GetImageEncoders(count, bytes, codecs) != Gdiplus::Ok)
            throw std::runtime_error(
                "Windows image codec: unable to enumerate encoders");

        for (UINT index = 0; index < count; ++index) {
            if (std::wcscmp(codecs[index].MimeType, mime_type) == 0)
                return codecs[index].Clsid;
        }
        throw std::runtime_error(
            "Windows image codec: requested encoder is unavailable");
    }

    std::vector<std::uint8_t> stream_bytes(IStream *stream) {
        STATSTG statistics = {};
        if (stream->Stat(&statistics, STATFLAG_NONAME) != S_OK ||
            statistics.cbSize.QuadPart >
                std::numeric_limits<std::size_t>::max()) {
            throw std::runtime_error(
                "Windows image codec: unable to size encoded data");
        }
        HGLOBAL memory = nullptr;
        if (GetHGlobalFromStream(stream, &memory) != S_OK)
            throw std::runtime_error(
                "Windows image codec: unable to access encoded data");
        const std::size_t size = static_cast<std::size_t>(
            statistics.cbSize.QuadPart);
        const void *source = GlobalLock(memory);
        if (!source && size != 0)
            throw std::runtime_error(
                "Windows image codec: unable to lock encoded data");
        std::vector<std::uint8_t> result(size);
        if (size != 0)
            std::memcpy(result.data(), source, size);
        if (source)
            GlobalUnlock(memory);
        return result;
    }

    void require_supported_input(
        const std::uint8_t *data,
        std::size_t size) {
        const bool png = size >= 8 &&
            std::memcmp(data, "\x89PNG\r\n\x1a\n", 8) == 0;
        const bool jpeg = size >= 2 && data[0] == 0xff && data[1] == 0xd8;
        if (!png && !jpeg)
            throw std::runtime_error(
                "Windows image codec: input is not PNG or JPEG");
    }
}

namespace native::detail
{
    decoded_image decode_image(
        const std::uint8_t *data,
        std::size_t size) {
        (void)runtime();
        require_supported_input(data, size);
        stream_ptr stream = input_stream(data, size);
        std::unique_ptr<Gdiplus::Bitmap> bitmap(
            Gdiplus::Bitmap::FromStream(stream.get(), FALSE));
        if (!bitmap || bitmap->GetLastStatus() != Gdiplus::Ok)
            throw std::runtime_error("Windows image codec: decode failed");

        const UINT width = bitmap->GetWidth();
        const UINT height = bitmap->GetHeight();
        if (width == 0 || height == 0 ||
            width > static_cast<UINT>(std::numeric_limits<coord>::max()) ||
            height > static_cast<UINT>(std::numeric_limits<coord>::max())) {
            throw std::runtime_error(
                "Windows image codec: dimensions exceed native image limits");
        }

        Gdiplus::Rect bounds(0, 0, width, height);
        Gdiplus::BitmapData locked = {};
        if (bitmap->LockBits(
                &bounds,
                Gdiplus::ImageLockModeRead,
                PixelFormat32bppARGB,
                &locked) != Gdiplus::Ok) {
            throw std::runtime_error(
                "Windows image codec: unable to read decoded pixels");
        }

        decoded_image decoded;
        decoded.width = static_cast<dim>(width);
        decoded.height = static_cast<dim>(height);
        decoded.pixels = std::make_unique<rgba[]>(
            static_cast<std::size_t>(width) * height);
        const auto *base = static_cast<const std::uint8_t *>(locked.Scan0);
        for (UINT y = 0; y < height; ++y) {
            const auto *row = base + static_cast<std::ptrdiff_t>(y) * locked.Stride;
            for (UINT x = 0; x < width; ++x) {
                decoded.pixels[static_cast<std::size_t>(y) * width + x] =
                    rgba(row[x * 4 + 2], row[x * 4 + 1], row[x * 4],
                         row[x * 4 + 3]);
            }
        }
        bitmap->UnlockBits(&locked);
        return decoded;
    }

    std::vector<std::uint8_t> encode_image(
        image_format format,
        const rgba *pixels,
        dim width,
        dim height,
        int jpeg_quality) {
        (void)runtime();
        std::vector<std::uint8_t> bgra(
            static_cast<std::size_t>(width) * height * 4);
        for (std::size_t index = 0;
             index < static_cast<std::size_t>(width) * height;
             ++index) {
            bgra[index * 4] = pixels[index].b;
            bgra[index * 4 + 1] = pixels[index].g;
            bgra[index * 4 + 2] = pixels[index].r;
            bgra[index * 4 + 3] = pixels[index].a;
        }

        Gdiplus::Bitmap bitmap(
            width,
            height,
            static_cast<INT>(width) * 4,
            PixelFormat32bppARGB,
            bgra.data());
        if (bitmap.GetLastStatus() != Gdiplus::Ok)
            throw std::runtime_error(
                "Windows image codec: unable to create image");

        stream_ptr stream = output_stream();
        const wchar_t *mime = format == image_format::png
            ? L"image/png"
            : L"image/jpeg";
        const CLSID encoder = encoder_clsid(mime);
        Gdiplus::EncoderParameters parameters = {};
        Gdiplus::EncoderParameters *parameter_ptr = nullptr;
        ULONG quality = static_cast<ULONG>(jpeg_quality);
        if (format == image_format::jpeg) {
            parameters.Count = 1;
            parameters.Parameter[0].Guid = Gdiplus::EncoderQuality;
            parameters.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
            parameters.Parameter[0].NumberOfValues = 1;
            parameters.Parameter[0].Value = &quality;
            parameter_ptr = &parameters;
        }

        if (bitmap.Save(stream.get(), &encoder, parameter_ptr) != Gdiplus::Ok)
            throw std::runtime_error("Windows image codec: encode failed");
        return stream_bytes(stream.get());
    }
}
