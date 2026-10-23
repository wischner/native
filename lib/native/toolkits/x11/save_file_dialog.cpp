//
// Presents the native Athena save file chooser in its owner-modal
// session. Placement and native grabs share the modal-shell policy.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native/save_file_dialog.h>
#include "file_dialog_fallback.h"
namespace native
{
    void save_file_dialog::show_native() {
        if (!begin_dialog())
            return;
        try {
            if (!linux::x11::show_file_dialog_fallback(
                    *this,
                    true,
                    false,
                    get_suggested_name(),
                    get_default_extension(),
                    get_confirm_overwrite()))
                on_native_cancel();
        } catch (...) {
            on_native_cancel();
            throw;
        }
    }
} // namespace native
