//
// Presents the GEM SDK extended selector for saving one file.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/save_file_dialog.h>

#include "file_dialog_common.h"

namespace native
{
    void save_file_dialog::show_native() {
        if (!begin_dialog())
            return;

        try {
            const auto response =
                linux::gemix::show_file_dialog(*this,
                                               get_suggested_name(),
                                               true,
                                               false,
                                               get_default_extension(),
                                               get_confirm_overwrite());

            if (response.accepted) {
                this->on_native_accept(
                    {response.path});
            } else {
                this
                    ->on_native_cancel();
            }
        } catch (...) {
            this->on_native_cancel();
            throw;
        }
    }
} // namespace native
