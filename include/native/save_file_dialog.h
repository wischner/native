//
// Declares the portable file-save dialog backed by each system's
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
    // Represents a native file-save chooser owned by an application.
    class save_file_dialog : public file_dialog
    {
    public:
        // Construct a file-save dialog with a default title.
        explicit save_file_dialog(
            app_wnd &owner, std::string title = "Save File");

        // Return the suggested leaf filename.
        const std::string &get_suggested_name() const;

        // Set the suggested leaf filename.
        save_file_dialog &set_suggested_name(
            const std::string &name);

        // Return the extension appended when the user supplies none.
        const std::string &get_default_extension() const;

        // Set the extension appended when the user supplies none.
        save_file_dialog &set_default_extension(
            const std::string &extension);

        // Return whether replacing a file requires confirmation.
        bool get_confirm_overwrite() const;

        // Enable or disable confirmation before replacing a file.
        save_file_dialog &set_confirm_overwrite(bool confirm);

        // Present the chooser after create(); completion emits the
        // inherited on_modal_close signal.
        void show() const override;

    private:
        std::string _suggested_name;
        std::string _default_extension;
        bool _confirm_overwrite;
    };
} // namespace native
