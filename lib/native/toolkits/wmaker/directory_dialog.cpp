//
// Presents the WINGs standard directory-selection panel.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/directory_dialog.h>

namespace linux::wmaker
{
    void show_file_dialog(native::file_dialog &dialog,
                          bool save,
                          bool directory = false);
}

namespace native
{
    void directory_dialog::show() const {
        if (!begin_dialog())
            return;
        try {
            linux::wmaker::show_file_dialog(
                *const_cast<directory_dialog *>(this), false, true);
        } catch (...) {
            const_cast<directory_dialog *>(this)->on_native_cancel();
            throw;
        }
    }
} // namespace native
