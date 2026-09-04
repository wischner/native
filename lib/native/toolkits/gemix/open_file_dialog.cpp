//
// Presents the standard GEM AES file selector for opening one file.
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
            const linux::gemix::file_dialog_response response =
                linux::gemix::show_file_dialog(*this, std::string());
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
