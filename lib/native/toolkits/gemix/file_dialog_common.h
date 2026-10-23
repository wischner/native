//
// Declares private helpers for GEM SDK selectors used by
// portable open and save dialogs.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>

#include <native/file_dialog.h>

namespace linux::gemix
{
    // Contains the selected file or directory path.
    struct file_dialog_response
    {
        bool accepted = false;
        std::string path;
    };

    // Use the SDK open/save/folder selector with its native modal loop.
    file_dialog_response
    show_file_dialog(native::file_dialog &dialog,
                     const std::string &suggested_name,
                     bool save = false,
                     bool directory_only = false,
                     const std::string &extension = {},
                     bool confirm_overwrite = false);

    // Append an extension when a selected filename has no suffix.
    std::string add_default_extension(
        const std::string &path, const std::string &extension);

} // namespace linux::gemix
