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
    void open_file_dialog::show() const {
        if (!begin_dialog())
            return;

        try {
            const linux::gemix::file_dialog_response response =
                linux::gemix::show_file_dialog(*this, std::string());
            if (response.accepted) {
                const_cast<open_file_dialog *>(this)->on_native_accept(
                    {response.path});
            } else {
                const_cast<open_file_dialog *>(this)
                    ->on_native_cancel();
            }
        } catch (...) {
            const_cast<open_file_dialog *>(this)->on_native_cancel();
            throw;
        }
    }
} // namespace native
