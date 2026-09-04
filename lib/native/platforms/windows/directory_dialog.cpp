//
// Presents the Windows Common Item directory dialog.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/directory_dialog.h>

#include "file_dialog_common.h"

namespace native
{
    void directory_dialog::show_native() {
        if (!begin_dialog())
            return;
        try {
            const auto response = windows::show_directory_dialog(
                *this, get_allow_multiple());
            auto *self = this;
            if (response.accepted)
                self->on_native_accept(response.paths);
            else
                self->on_native_cancel();
        } catch (...) {
            this->on_native_cancel();
            throw;
        }
    }
} // namespace native
