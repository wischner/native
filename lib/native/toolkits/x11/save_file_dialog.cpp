//
// Presents the Linux desktop file-save chooser for the Athena backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/save_file_dialog.h>

#include <stdexcept>

#include "../../platforms/linux/file_dialog_process.h"

namespace native
{
    void save_file_dialog::show() const {
        if (!begin_dialog())
            return;

        try {
            linux::file_dialog_response response =
                linux::show_save_file_dialog(*this,
                                             get_suggested_name(),
                                             get_confirm_overwrite());
            if (response.outcome ==
                    linux::file_dialog_outcome::accepted &&
                !response.paths.empty()) {
                response.paths.front() = linux::add_default_extension(
                    response.paths.front(), get_default_extension());
                const_cast<save_file_dialog *>(this)->on_native_accept(
                    response.paths);
            } else {
                const_cast<save_file_dialog *>(this)
                    ->on_native_cancel();
            }

            if (response.outcome ==
                linux::file_dialog_outcome::unavailable) {
                throw std::runtime_error(
                    "X11: Install Zenity or KDialog to show file "
                    "dialogs.");
            }
        } catch (...) {
            const_cast<save_file_dialog *>(this)->on_native_cancel();
            throw;
        }
    }
} // namespace native
