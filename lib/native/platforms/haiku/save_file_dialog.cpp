//
// Presents the Haiku standard save-file panel and leaves completion to
// the asynchronous BeAPI message adapter.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/save_file_dialog.h>

#include "file_dialog_common.h"

namespace native
{
    void save_file_dialog::show() const {
        if (!begin_dialog())
            return;

        try {
            haiku::show_file_dialog(
                *const_cast<save_file_dialog *>(this),
                true,
                false,
                get_suggested_name(),
                get_default_extension());
        } catch (...) {
            const_cast<save_file_dialog *>(this)->on_native_cancel();
            throw;
        }
    }
} // namespace native
