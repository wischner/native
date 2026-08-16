//
// Presents and completes the Windows standard file-open dialog.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/open_file_dialog.h>

#include "file_dialog_common.h"

namespace native
{
    void open_file_dialog::show() const {
        if (!begin_dialog())
            return;

        try {
            const windows::file_dialog_response response =
                windows::show_open_file_dialog(
                    *this, get_allow_multiple());
            if (response.accepted) {
                const_cast<open_file_dialog *>(this)->on_native_accept(
                    response.paths);
            } else {
                const_cast<open_file_dialog *>(this)
                    ->on_native_cancel();
            }
        } catch (...) {
            const_cast<open_file_dialog *>(this)->on_native_cancel();
            throw;
        }
    }
} // namespace native
