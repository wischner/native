//
// Presents a standard desktop directory chooser for Athena/X11.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/directory_dialog.h>

#include "../../platforms/linux/file_dialog_process.h"
#include "file_dialog_fallback.h"

namespace native
{
    void directory_dialog::show_native() {
        if (!begin_dialog())
            return;
        try {
            const auto response = linux::show_directory_dialog(
                *this, get_allow_multiple());
            auto *self = this;
            if (response.outcome == linux::file_dialog_outcome::accepted)
                self->on_native_accept(response.paths);
            else if (response.outcome ==
                     linux::file_dialog_outcome::cancelled)
                self->on_native_cancel();
            else if (!linux::x11::show_file_dialog_fallback(
                         *this,
                         false,
                         true,
                         std::string(),
                         std::string(),
                         false)) {
                self->on_native_cancel();
            }
        } catch (...) {
            this->on_native_cancel();
            throw;
        }
    }
} // namespace native
