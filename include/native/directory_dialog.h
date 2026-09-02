//
// Declares the portable standard directory-selection dialog.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>

#include "file_dialog.h"

namespace native
{
    class directory_dialog : public file_dialog
    {
    public:
        // Construct a folder chooser borrowing its application owner.
        explicit directory_dialog(
            app_wnd &owner, std::string title = "Select Folder");

        // Return whether the chooser requests multiple folders.
        bool get_allow_multiple() const;

        // Configure optional multiple-folder selection.
        directory_dialog &set_allow_multiple(bool allow_multiple);

        // Present the platform's standard directory chooser.
        void show() const override;

    private:
        bool _allow_multiple = false;
    };
} // namespace native
