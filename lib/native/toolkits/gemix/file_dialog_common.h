//
// Declares private helpers around GEM AES's standard file selector for
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
    // Contains the single path returned by the AES file selector.
    struct file_dialog_response
    {
        bool accepted = false;
        std::string path;
    };

    // Show the AES file selector with an optional suggested filename.
    file_dialog_response show_file_dialog(
        const native::file_dialog &dialog,
        const std::string &suggested_name);

    // Append an extension when a selected filename has no suffix.
    std::string add_default_extension(
        const std::string &path, const std::string &extension);

    // Ask AES for overwrite confirmation when the selected file exists.
    bool confirm_file_overwrite(const std::string &path);
} // namespace linux::gemix
