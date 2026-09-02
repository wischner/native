//
// Presents the standard WINGs open-file panel.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/open_file_dialog.h>

namespace linux::wmaker
{
    void show_file_dialog(native::file_dialog &dialog,
                          bool save,
                          bool directory = false);
}

namespace native
{
    void open_file_dialog::show() const {
        if (!begin_dialog())
            return;
        try {
            linux::wmaker::show_file_dialog(
                *const_cast<open_file_dialog *>(this), false);
        } catch (...) {
            const_cast<open_file_dialog *>(this)->on_native_cancel();
            throw;
        }
    }
} // namespace native
