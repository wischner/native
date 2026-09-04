//
// Implements Tracker file icons and Haiku special-directory discovery.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "../../filesystem_backend.h"

#include <Bitmap.h>
#include <FindDirectory.h>
#include <Node.h>
#include <NodeInfo.h>
#include <Path.h>

#include <cstdint>
#include <filesystem>
#include <utility>
#include <vector>

namespace
{
    using directory_kind = native::special_directory_kind;

    std::filesystem::path find_special(directory_which which) {
        BPath value;
        return find_directory(which, &value) == B_OK && value.Path()
                   ? std::filesystem::path(value.Path())
                   : std::filesystem::path();
    }

    void append(
        std::vector<native::detail::special_directory_path> &result,
        directory_kind kind,
        std::filesystem::path path) {
        if (!path.empty())
            result.push_back({kind, std::move(path)});
    }
} // namespace

namespace native::detail
{
    bool load_native_file_icon(const std::filesystem::path &path,
                               bool,
                               img &target) {
        if (path.empty())
            return false;
        const std::string value = path.string();
        BNode node(value.c_str());
        if (node.InitCheck() != B_OK)
            return false;

        constexpr int icon_extent = 32;
        BBitmap bitmap(BRect(0,
                             0,
                             icon_extent - 1,
                             icon_extent - 1),
                       B_RGBA32);
        BNodeInfo information(&node);
        if (bitmap.InitCheck() != B_OK ||
            information.InitCheck() != B_OK ||
            information.GetTrackerIcon(&bitmap, B_LARGE_ICON) != B_OK) {
            return false;
        }

        img source(icon_extent, icon_extent);
        const auto *bits = static_cast<const std::uint8_t *>(bitmap.Bits());
        for (int y = 0; y < icon_extent; ++y) {
            const std::uint8_t *row =
                bits + static_cast<std::size_t>(y) * bitmap.BytesPerRow();
            for (int x = 0; x < icon_extent; ++x) {
                source.pixels()[y * icon_extent + x] = rgba(
                    row[x * 4 + 2],
                    row[x * 4 + 1],
                    row[x * 4],
                    row[x * 4 + 3]);
            }
        }
        target.get_gpx()
            .clear(rgba(0, 0, 0, 0))
            .draw_img(source,
                      rect(0, 0, target.w(), target.h()),
                      image_filter::linear);
        return true;
    }

    std::vector<special_directory_path>
    detect_platform_special_directories() {
        std::vector<special_directory_path> result;
        const std::filesystem::path home =
            find_special(B_USER_DIRECTORY);
        append(result, directory_kind::home, home);
        append(result,
               directory_kind::desktop,
               find_special(B_DESKTOP_DIRECTORY));
        append(result, directory_kind::documents, home / "Documents");
        append(result, directory_kind::downloads, home / "Downloads");
        append(result, directory_kind::music, home / "Music");
        append(result, directory_kind::pictures, home / "Pictures");
        append(result, directory_kind::videos, home / "Videos");
        append(result, directory_kind::public_share, home / "Public");
        append(result, directory_kind::templates, home / "Templates");
        append(result,
               directory_kind::applications,
               find_special(B_APPS_DIRECTORY));
        append(result,
               directory_kind::fonts,
               find_special(B_USER_FONTS_DIRECTORY));
        append(result,
               directory_kind::configuration,
               find_special(B_USER_CONFIG_DIRECTORY));
        append(result,
               directory_kind::application_data,
               find_special(B_USER_DATA_DIRECTORY));
        append(result,
               directory_kind::cache,
               find_special(B_USER_CACHE_DIRECTORY));
        return result;
    }
} // namespace native::detail
