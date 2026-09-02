//
// Declares private adapters for configuring and reading the Windows
// Common Item Dialog implementations used for open and save.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>
#include <vector>

#include <native/file_dialog.h>

namespace windows
{
    // Contains the result returned by a Windows common file dialog.
    struct file_dialog_response
    {
        bool accepted = false;
        std::vector<std::string> paths;
    };

    // Show the Windows standard open-file dialog.
    file_dialog_response show_open_file_dialog(
        const native::file_dialog &dialog, bool allow_multiple);

    // Show the Windows standard directory-selection dialog.
    file_dialog_response show_directory_dialog(
        const native::file_dialog &dialog, bool allow_multiple);

    // Show the Windows standard save-file dialog.
    file_dialog_response show_save_file_dialog(
        const native::file_dialog &dialog,
        const std::string &suggested_name,
        const std::string &default_extension,
        bool confirm_overwrite);
} // namespace windows
