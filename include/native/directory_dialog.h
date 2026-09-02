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
        explicit directory_dialog(
            app_wnd &owner, std::string title = "Select Folder");

        bool get_allow_multiple() const;
        directory_dialog &set_allow_multiple(bool allow_multiple);

        // Present the platform's standard directory chooser.
        void show() const override;

    private:
        bool _allow_multiple = false;
    };
} // namespace native
