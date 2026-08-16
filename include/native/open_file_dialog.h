//
// Declares the portable file-open dialog backed by each system's
// standard chooser whenever that system provides one.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <string>

#include "file_dialog.h"

namespace native
{
    // Represents a native file-open chooser owned by an application.
    class open_file_dialog : public file_dialog
    {
    public:
        // Construct a file-open dialog with a default title.
        explicit open_file_dialog(
            app_wnd &owner, std::string title = "Open File");

        // Return whether the user may select more than one file.
        bool get_allow_multiple() const;

        // Enable or disable selecting more than one file.
        open_file_dialog &set_allow_multiple(bool allow_multiple);

        // Present the chooser after create(); completion emits the
        // inherited on_modal_close signal.
        void show() const override;

    private:
        bool _allow_multiple;
    };
} // namespace native
