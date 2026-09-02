//
// Presents the OpenLook standard directory-selection chooser.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/directory_dialog.h>

namespace linux::openlook
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
            linux::openlook::show_file_dialog(
                *const_cast<directory_dialog *>(this), false, true);
        } catch (...) {
            const_cast<directory_dialog *>(this)->on_native_cancel();
            throw;
        }
    }
} // namespace native
