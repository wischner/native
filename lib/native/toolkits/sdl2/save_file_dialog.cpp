//
// Presents the library-owned modern file-save chooser for SDL2.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/save_file_dialog.h>

#include "../../platforms/linux/file_dialog_process.h"
#include "globals.h"

namespace native
{
    void save_file_dialog::show_native() {
        if (!begin_dialog())
            return;

        app_wnd *owner = get_owner();

        try {
            std::vector<std::filesystem::path> paths;
            if (linux::sdl2::show_file_dialog(
                    *this,
                    true,
                    false,
                    get_suggested_name(),
                    get_default_extension(),
                    get_confirm_overwrite(),
                    paths) &&
                !paths.empty()) {
                paths.front() = linux::add_default_extension(
                    paths.front(), get_default_extension());
                this->on_native_accept(paths);
            } else {
                this->on_native_cancel();
            }
            linux::sdl2::restore_window_focus(owner);
        } catch (...) {
            this->on_native_cancel();
            linux::sdl2::restore_window_focus(owner);
            throw;
        }
    }
} // namespace native
