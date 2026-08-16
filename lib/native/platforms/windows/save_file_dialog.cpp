//
// Presents and completes the Windows standard file-save dialog.
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
            const windows::file_dialog_response response =
                windows::show_save_file_dialog(
                    *this,
                    get_suggested_name(),
                    get_default_extension(),
                    get_confirm_overwrite());
            if (response.accepted) {
                const_cast<save_file_dialog *>(this)->on_native_accept(
                    response.paths);
            } else {
                const_cast<save_file_dialog *>(this)
                    ->on_native_cancel();
            }
        } catch (...) {
            const_cast<save_file_dialog *>(this)->on_native_cancel();
            throw;
        }
    }
} // namespace native
