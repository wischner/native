//
// Presents the standard GEM AES file selector for saving one file.
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
            linux::gemix::file_dialog_response response =
                linux::gemix::show_file_dialog(
                    *this, get_suggested_name());
            if (response.accepted) {
                response.path = linux::gemix::add_default_extension(
                    response.path, get_default_extension());
                if (get_confirm_overwrite() &&
                    !linux::gemix::confirm_file_overwrite(
                        response.path)) {
                    response.accepted = false;
                }
            }

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
