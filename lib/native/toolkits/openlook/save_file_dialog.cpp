//
// Presents the native XView file-save chooser.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/save_file_dialog.h>

namespace linux::openlook
{
    void show_file_dialog(native::file_dialog &dialog,
                          bool save,
                          bool directory = false);
}

namespace native
{
    void save_file_dialog::show() const {
        if (!begin_dialog())
            return;
        try {
            linux::openlook::show_file_dialog(
                *const_cast<save_file_dialog *>(this), true);
        } catch (...) {
            const_cast<save_file_dialog *>(this)->on_native_cancel();
            throw;
        }
    }
} // namespace native
