//
// Implements Windows Shell file icons and Known Folder discovery.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif

#include "../../filesystem_backend.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <vector>

#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>
#include <windows.h>

namespace
{
    using directory_kind = native::special_directory_kind;

    class owned_icon final
    {
    public:
        explicit owned_icon(HICON value)
            : _value(value) {}

        ~owned_icon() {
            if (_value)
                DestroyIcon(_value);
        }

        owned_icon(const owned_icon &) = delete;
        owned_icon &operator=(const owned_icon &) = delete;

        HICON get() const {
            return _value;
        }

    private:
        HICON _value;
    };

    bool render_icon(HICON icon, native::img &target) {
        HDC device = CreateCompatibleDC(nullptr);
        if (!device)
            return false;

        BITMAPV5HEADER header{};
        header.bV5Size = sizeof(header);
        header.bV5Width = target.w();
        header.bV5Height = -target.h();
        header.bV5Planes = 1;
        header.bV5BitCount = 32;
        header.bV5Compression = BI_BITFIELDS;
        header.bV5RedMask = 0x00ff0000;
        header.bV5GreenMask = 0x0000ff00;
        header.bV5BlueMask = 0x000000ff;
        header.bV5AlphaMask = 0xff000000;
        void *bits = nullptr;
        HBITMAP bitmap = CreateDIBSection(
            device,
            reinterpret_cast<BITMAPINFO *>(&header),
            DIB_RGB_COLORS,
            &bits,
            nullptr,
            0);
        if (!bitmap || !bits) {
            if (bitmap)
                DeleteObject(bitmap);
            DeleteDC(device);
            return false;
        }

        const std::size_t pixel_count =
            static_cast<std::size_t>(target.w()) * target.h();
        std::memset(bits, 0, pixel_count * 4);
        HGDIOBJ previous = SelectObject(device, bitmap);
        const BOOL drawn = DrawIconEx(device,
                                      0,
                                      0,
                                      icon,
                                      target.w(),
                                      target.h(),
                                      0,
                                      nullptr,
                                      DI_NORMAL);
        if (drawn) {
            const auto *source = static_cast<const std::uint8_t *>(bits);
            bool has_alpha = false;
            for (std::size_t index = 0; index < pixel_count; ++index) {
                has_alpha = has_alpha || source[index * 4 + 3] != 0;
                target.pixels()[index] = native::rgba(
                    source[index * 4 + 2],
                    source[index * 4 + 1],
                    source[index * 4],
                    source[index * 4 + 3]);
            }
            if (has_alpha) {
                for (std::size_t index = 0;
                     index < pixel_count;
                     ++index) {
                    native::rgba &pixel = target.pixels()[index];
                    if (pixel.a == 0)
                        continue;
                    const auto unpremultiply = [&pixel](unsigned channel) {
                        return static_cast<std::uint8_t>(std::min(
                            255U,
                            (channel * 255U + pixel.a / 2U) / pixel.a));
                    };
                    pixel.r = unpremultiply(pixel.r);
                    pixel.g = unpremultiply(pixel.g);
                    pixel.b = unpremultiply(pixel.b);
                }
            } else {
                void *white_bits = nullptr;
                HBITMAP white_bitmap = CreateDIBSection(
                    device,
                    reinterpret_cast<BITMAPINFO *>(&header),
                    DIB_RGB_COLORS,
                    &white_bits,
                    nullptr,
                    0);
                if (white_bitmap && white_bits) {
                    std::memset(white_bits, 0xff, pixel_count * 4);
                    SelectObject(device, white_bitmap);
                    DrawIconEx(device,
                               0,
                               0,
                               icon,
                               target.w(),
                               target.h(),
                               0,
                               nullptr,
                               DI_NORMAL);
                    const auto *white =
                        static_cast<const std::uint8_t *>(white_bits);
                    for (std::size_t index = 0;
                         index < pixel_count;
                         ++index) {
                        const unsigned blue = source[index * 4];
                        const unsigned green = source[index * 4 + 1];
                        const unsigned red = source[index * 4 + 2];
                        const auto channel_difference = [](
                            unsigned foreground,
                            unsigned background) {
                            return background > foreground
                                       ? background - foreground
                                       : 0U;
                        };
                        const unsigned difference =
                            (channel_difference(
                                 blue, white[index * 4]) +
                             channel_difference(
                                 green, white[index * 4 + 1]) +
                             channel_difference(
                                 red, white[index * 4 + 2])) /
                            3U;
                        const unsigned alpha = 255U -
                                               std::min(255U, difference);
                        const auto unblend = [alpha](unsigned channel) {
                            return static_cast<std::uint8_t>(
                                alpha == 0
                                    ? 0
                                    : std::min(
                                          255U,
                                          (channel * 255U + alpha / 2U) /
                                              alpha));
                        };
                        target.pixels()[index] = native::rgba(
                            unblend(red),
                            unblend(green),
                            unblend(blue),
                            static_cast<std::uint8_t>(alpha));
                    }
                    SelectObject(device, bitmap);
                } else {
                    for (std::size_t index = 0;
                         index < pixel_count;
                         ++index) {
                        native::rgba &pixel = target.pixels()[index];
                        pixel.a = pixel.r || pixel.g || pixel.b ? 255 : 0;
                    }
                }
                if (white_bitmap)
                    DeleteObject(white_bitmap);
            }
        }
        SelectObject(device, previous);
        DeleteObject(bitmap);
        DeleteDC(device);
        return drawn != FALSE;
    }

    void append_known_folder(
        std::vector<native::detail::special_directory_path> &result,
        directory_kind kind,
        REFKNOWNFOLDERID identifier) {
        PWSTR value = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(
                identifier, KF_FLAG_DONT_VERIFY, nullptr, &value)) &&
            value && *value) {
            result.push_back({kind, std::filesystem::path(value)});
        }
        CoTaskMemFree(value);
    }
} // namespace

namespace native::detail
{
    bool load_native_file_icon(const std::filesystem::path &path,
                               bool directory,
                               img &target) {
        std::error_code error;
        const bool exists = std::filesystem::exists(path, error);
        SHFILEINFOW information{};
        UINT flags = SHGFI_ICON |
                     (target.w() <= 16 ? SHGFI_SMALLICON
                                       : SHGFI_LARGEICON);
        DWORD attributes = directory ? FILE_ATTRIBUTE_DIRECTORY
                                     : FILE_ATTRIBUTE_NORMAL;
        if (!exists)
            flags |= SHGFI_USEFILEATTRIBUTES;
        const std::wstring value = path.empty()
                                       ? std::wstring(L"native.file")
                                       : path.wstring();
        if (SHGetFileInfoW(value.c_str(),
                           attributes,
                           &information,
                           sizeof(information),
                           flags) == 0 ||
            !information.hIcon) {
            return false;
        }
        const owned_icon icon(information.hIcon);
        return render_icon(icon.get(), target);
    }

    std::vector<special_directory_path>
    detect_platform_special_directories() {
        std::vector<special_directory_path> result;
        append_known_folder(result, directory_kind::home, FOLDERID_Profile);
        append_known_folder(
            result, directory_kind::desktop, FOLDERID_Desktop);
        append_known_folder(
            result, directory_kind::documents, FOLDERID_Documents);
        append_known_folder(
            result, directory_kind::downloads, FOLDERID_Downloads);
        append_known_folder(result, directory_kind::music, FOLDERID_Music);
        append_known_folder(
            result, directory_kind::pictures, FOLDERID_Pictures);
        append_known_folder(result, directory_kind::videos, FOLDERID_Videos);
        append_known_folder(
            result, directory_kind::public_share, FOLDERID_Public);
        append_known_folder(
            result, directory_kind::templates, FOLDERID_Templates);
        append_known_folder(
            result, directory_kind::applications, FOLDERID_Programs);
        append_known_folder(result, directory_kind::fonts, FOLDERID_Fonts);
        append_known_folder(result,
                            directory_kind::configuration,
                            FOLDERID_RoamingAppData);
        append_known_folder(result,
                            directory_kind::application_data,
                            FOLDERID_LocalAppData);
        append_known_folder(
            result, directory_kind::cache, FOLDERID_LocalAppData);
        return result;
    }
} // namespace native::detail
