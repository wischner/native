//
// Presents a standard desktop directory chooser for SDL2.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/directory_dialog.h>

#include "../../platforms/linux/file_dialog_process.h"

namespace native
{
    void directory_dialog::show() const {
        if (!begin_dialog())
            return;
        try {
            const auto response = linux::show_directory_dialog(
                *this, get_allow_multiple());
            auto *self = const_cast<directory_dialog *>(this);
            if (response.outcome == linux::file_dialog_outcome::accepted)
                self->on_native_accept(response.paths);
            else
                self->on_native_cancel();
        } catch (...) {
            const_cast<directory_dialog *>(this)->on_native_cancel();
            throw;
        }
    }
} // namespace native
