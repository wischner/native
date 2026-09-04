//
// Presents the standard Haiku directory panel.
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
            haiku::show_file_dialog(
                *this,
                false,
                get_allow_multiple(),
                {},
                {},
                true);
        } catch (...) {
            this->on_native_cancel();
            throw;
        }
    }
} // namespace native
