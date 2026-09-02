//
// Presents the Motif standard directory-selection dialog.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/directory_dialog.h>

#include "file_dialog_common.h"

namespace native
{
    void directory_dialog::show() const {
        if (!begin_dialog())
            return;
        try {
            linux::openmotif::show_file_dialog(
                *const_cast<directory_dialog *>(this), false, true);
        } catch (...) {
            const_cast<directory_dialog *>(this)->on_native_cancel();
            throw;
        }
    }
} // namespace native
