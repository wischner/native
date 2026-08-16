//
// Declares the self-contained SDL file chooser used when the desktop
// does not provide Zenity or KDialog.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>

#include <native/file_dialog.h>

#include "../../platforms/linux/file_dialog_process.h"

namespace linux::sdl2
{
    // Run an owner-modal SDL file chooser and return its final outcome.
    linux::file_dialog_response show_file_dialog_fallback(
        const native::file_dialog &dialog,
        bool save,
        bool allow_multiple,
        const std::string &suggested_name,
        const std::string &default_extension,
        bool confirm_overwrite);
} // namespace linux::sdl2
