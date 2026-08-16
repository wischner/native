//
// Presents the standard WINGs save-file panel.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/save_file_dialog.h>

namespace linux::wmaker
{
    void show_file_dialog(native::file_dialog &dialog, bool save);
}

namespace native
{
    void save_file_dialog::show() const {
        if (!begin_dialog())
            return;
        try {
            linux::wmaker::show_file_dialog(
                *const_cast<save_file_dialog *>(this), true);
        } catch (...) {
            const_cast<save_file_dialog *>(this)->on_native_cancel();
            throw;
        }
    }
} // namespace native
