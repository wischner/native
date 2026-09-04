//
// Presents the Haiku standard open-file panel and leaves completion to
// the asynchronous BeAPI message adapter.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/open_file_dialog.h>

#include "file_dialog_common.h"

namespace native
{
    void open_file_dialog::show_native() {
        if (!begin_dialog())
            return;

        try {
            haiku::show_file_dialog(
                *this,
                false,
                get_allow_multiple(),
                std::string(),
                std::string());
        } catch (...) {
            this->on_native_cancel();
            throw;
        }
    }
} // namespace native
