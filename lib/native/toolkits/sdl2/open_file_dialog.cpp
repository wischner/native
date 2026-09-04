//
// Presents the library-owned modern file-open chooser for SDL2.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/open_file_dialog.h>

#include "globals.h"

namespace native
{
    void open_file_dialog::show_native() {
        if (!begin_dialog())
            return;

        app_wnd *owner = get_owner();

        try {
            std::vector<std::filesystem::path> paths;
            if (linux::sdl2::show_file_dialog(
                    *this,
                    false,
                    false,
                    std::string(),
                    std::string(),
                    false,
                    paths)) {
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
