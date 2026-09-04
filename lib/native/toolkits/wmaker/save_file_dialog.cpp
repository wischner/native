//
// Presents the standard WINGs save-file panel.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/save_file_dialog.h>

namespace linux::wmaker
{
    void show_file_dialog(native::file_dialog &dialog,
                          bool save,
                          bool directory = false);
}

namespace native
{
    void save_file_dialog::show_native() {
        if (!begin_dialog())
            return;
        try {
            linux::wmaker::show_file_dialog(
                *this, true);
        } catch (...) {
            this->on_native_cancel();
            throw;
        }
    }
} // namespace native
