//
// Declares the private platform boundary for file icons and known folders.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <filesystem>
#include <vector>

#include <native/filesystem.h>
#include <native/graphics.h>

namespace native::detail
{
    // Carries one platform-known path into the shared value model.
    struct special_directory_path
    {
        special_directory_kind kind;
        std::filesystem::path path;
    };

    // Render a platform icon into an exact-size transparent image.
    bool load_native_file_icon(const std::filesystem::path &path,
                               bool directory,
                               img &target);

    // Return special paths supplied by the current platform.
    std::vector<special_directory_path>
    detect_platform_special_directories();
} // namespace native::detail
