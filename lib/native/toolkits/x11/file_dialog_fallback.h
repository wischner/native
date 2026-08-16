//
// Declares the Athena file chooser used when no Linux desktop chooser
// process is installed. The implementation remains backend-private.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>

namespace native
{
    class file_dialog;
}

namespace linux::x11
{
    // Show an Athena path browser inside the active modal session;
    // return false when the owner has no usable Xt shell.
    bool show_file_dialog_fallback(
        native::file_dialog &dialog,
        bool save,
        const std::string &suggested_name,
        const std::string &default_extension,
        bool confirm_overwrite);
}
