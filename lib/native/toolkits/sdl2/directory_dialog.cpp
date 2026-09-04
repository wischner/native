//
// Presents the library-owned modern directory chooser for SDL2.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/directory_dialog.h>

#include "globals.h"

namespace native
{
    void directory_dialog::show_native() {
        if (!begin_dialog())
            return;
        app_wnd *owner = get_owner();
        try {
            auto *self = this;
            std::vector<std::filesystem::path> paths;
            if (linux::sdl2::show_file_dialog(
                    *this,
                    false,
                    true,
                    std::string(),
                    std::string(),
                    false,
                    paths)) {
                self->on_native_accept(paths);
            } else {
                self->on_native_cancel();
            }
            linux::sdl2::restore_window_focus(owner);
        } catch (...) {
            this->on_native_cancel();
            linux::sdl2::restore_window_focus(owner);
            throw;
        }
    }
} // namespace native
