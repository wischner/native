//
// Presents the native Athena directory chooser in its owner-modal
// session. Placement and native grabs share the modal-shell policy.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native/directory_dialog.h>
#include "file_dialog_fallback.h"
namespace native
{
    void directory_dialog::show_native() {
        if (!begin_dialog())
            return;
        try {
            if (!linux::x11::show_file_dialog_fallback(
                    *this, false, true, {}, {}, false))
                on_native_cancel();
        } catch (...) {
            on_native_cancel();
            throw;
        }
    }
} // namespace native
