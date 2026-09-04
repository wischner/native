//
// Adapts the AES standard selector to directory selection.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <filesystem>

#include <native/directory_dialog.h>

#include "file_dialog_common.h"

namespace native
{
    void directory_dialog::show_native() {
        if (!begin_dialog())
            return;
        try {
            const auto response = linux::gemix::show_file_dialog(
                *this, std::string());
            auto *self = this;
            if (!response.accepted) {
                self->on_native_cancel();
                return;
            }
            const std::filesystem::path selected(response.path);
            const std::string folder = selected.parent_path().string();
            if (folder.empty())
                self->on_native_cancel();
            else
                self->on_native_accept({folder});
        } catch (...) {
            this->on_native_cancel();
            throw;
        }
    }
} // namespace native
