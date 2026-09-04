//
// Adapts Native's file-dialog lifecycle to the shared private SDL2 browser.
// Dummy video drivers continue to report the chooser as unavailable.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <SDL2/SDL.h>

#include <filesystem>
#include <string>
#include <vector>

#include <native/file_dialog.h>

#include "file_browser.h"

namespace linux::sdl2
{
    bool show_file_dialog(
        native::file_dialog &dialog,
        bool save,
        bool directory,
        const std::string &suggested_name,
        const std::string &default_extension,
        bool confirm_overwrite,
        std::vector<std::filesystem::path> &paths) {
        const char *driver = SDL_GetCurrentVideoDriver();
        if (!driver || std::string(driver) == "dummy")
            return false;
        file_browser browser(dialog, save, directory,
                             suggested_name, default_extension,
                             confirm_overwrite);
        return browser.run(paths);
    }
} // namespace linux::sdl2

namespace native
{
    void file_dialog::cancel_native_dialog() {}
} // namespace native
