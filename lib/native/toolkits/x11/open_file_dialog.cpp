//
// Presents the Linux desktop file-open chooser for the Athena backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/open_file_dialog.h>

#include "../../platforms/linux/file_dialog_process.h"
#include "file_dialog_fallback.h"

namespace native
{
    void open_file_dialog::show() const {
        if (!begin_dialog())
            return;

        try {
            const linux::file_dialog_response response =
                linux::show_open_file_dialog(*this,
                                             get_allow_multiple());
            if (response.outcome ==
                linux::file_dialog_outcome::accepted) {
                const_cast<open_file_dialog *>(this)->on_native_accept(
                    response.paths);
            } else if (response.outcome ==
                       linux::file_dialog_outcome::cancelled) {
                const_cast<open_file_dialog *>(this)
                    ->on_native_cancel();
            } else if (!linux::x11::show_file_dialog_fallback(
                           *const_cast<open_file_dialog *>(this),
                           false,
                           std::string(),
                           std::string(),
                           false)) {
                const_cast<open_file_dialog *>(this)
                    ->on_native_cancel();
            }
        } catch (...) {
            const_cast<open_file_dialog *>(this)->on_native_cancel();
            throw;
        }
    }
} // namespace native
