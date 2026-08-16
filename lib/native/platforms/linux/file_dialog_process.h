//
// Declares the Linux desktop chooser process adapter used by toolkits
// that do not provide their own standard file-selection panel.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>
#include <vector>

#include <native/file_dialog.h>

namespace linux
{
    // Describes the outcome returned by a desktop file chooser.
    enum class file_dialog_outcome
    {
        accepted,
        cancelled,
        unavailable
    };

    // Contains a desktop chooser outcome and its selected paths.
    struct file_dialog_response
    {
        file_dialog_outcome outcome =
            file_dialog_outcome::unavailable;
        std::vector<std::string> paths;
    };

    // Show a desktop open chooser through Zenity or KDialog.
    file_dialog_response show_open_file_dialog(
        const native::file_dialog &dialog, bool allow_multiple);

    // Show a desktop save chooser through Zenity or KDialog.
    file_dialog_response show_save_file_dialog(
        const native::file_dialog &dialog,
        const std::string &suggested_name,
        bool confirm_overwrite);

    // Append an extension when a selected filename has no suffix.
    std::string add_default_extension(
        const std::string &path, const std::string &extension);
} // namespace linux
