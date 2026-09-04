//
// Presents the standard Motif open-file selection dialog.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/open_file_dialog.h>

#include "file_dialog_common.h"

namespace native
{
    void open_file_dialog::show_native() {
        if (!begin_dialog())
            return;

        try {
            linux::openmotif::show_file_dialog(
                *this, false);
        } catch (...) {
            this->on_native_cancel();
            throw;
        }
    }
} // namespace native
