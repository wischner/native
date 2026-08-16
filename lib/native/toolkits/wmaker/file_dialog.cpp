//
// Adapts the standard WINGs file panel to the portable modal-dialog
// result contract. WINGs owns folder navigation and file presentation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <string>

#include <WINGs/WINGs.h>

#include <native/file_dialog.h>
#include <native/save_file_dialog.h>

#include "globals.h"

namespace
{
    // WINGs deliberately keeps WMFilePanel opaque, but its stable ABI
    // starts with the panel window. This title is necessary for owned
    // panels because WINGs 0.96 names only ownerless panels itself.
    struct file_panel_prefix
    {
        WMWindow *window;
    };

    std::string initial_path(native::file_dialog &dialog,
                             bool save) {
        std::filesystem::path path =
            dialog.get_initial_path().empty()
                ? std::filesystem::path(".")
                : std::filesystem::path(
                      dialog.get_initial_path());
        if (save) {
            auto *save_dialog =
                dynamic_cast<native::save_file_dialog *>(&dialog);
            if (save_dialog &&
                !save_dialog->get_suggested_name().empty()) {
                path /= save_dialog->get_suggested_name();
            }
        }
        return path.string();
    }

    std::string add_default_extension(
        std::string path,
        const std::string &extension) {
        if (path.empty() || extension.empty() ||
            std::filesystem::path(path).has_extension()) {
            return path;
        }
        if (extension.front() != '.')
            path.push_back('.');
        path += extension;
        return path;
    }
} // namespace

namespace linux::wmaker
{
    void show_file_dialog(native::file_dialog &dialog, bool save) {
        native::app_wnd *owner = dialog.get_owner();
        window_state *owner_state = owner ? state(owner) : nullptr;
        if (!owner_state || !owner_state->window) {
            throw std::runtime_error(
                "Window Maker/WINGs: file dialog has no owner.");
        }

        WMFilePanel *panel = save
                                 ? reinterpret_cast<WMFilePanel *>(
                                       WMGetSavePanel(screen))
                                 : reinterpret_cast<WMFilePanel *>(
                                       WMGetOpenPanel(screen));
        if (!panel) {
            dialog.on_native_cancel();
            return;
        }
        auto *panel_prefix =
            reinterpret_cast<file_panel_prefix *>(panel);
        WMSetWindowTitle(panel_prefix->window,
                         dialog.get_title().c_str());
        WMSetFilePanelCanChooseFiles(panel, True);
        WMSetFilePanelCanChooseDirectories(panel, False);
        WMSetFilePanelAutoCompletion(panel, True);
        const std::string path = initial_path(dialog, save);
        const int accepted = WMRunModalFilePanelForDirectory(
            panel,
            owner_state->window,
            path.c_str(),
            dialog.get_title().c_str(),
            nullptr);
        if (!accepted) {
            dialog.on_native_cancel();
            return;
        }

        char *selected = WMGetFilePanelFileName(panel);
        std::string result = selected ? selected : "";
        if (selected)
            std::free(selected);
        if (save) {
            auto *save_dialog =
                dynamic_cast<native::save_file_dialog *>(&dialog);
            if (save_dialog) {
                result = add_default_extension(
                    std::move(result),
                    save_dialog->get_default_extension());
            }
        }
        if (result.empty())
            dialog.on_native_cancel();
        else
            dialog.on_native_accept({result});
    }
} // namespace linux::wmaker

namespace native
{
    void file_dialog::cancel_native_dialog() const {
        // The modal file-panel call closes its shared panel before
        // returning. There is no retained per-dialog resource.
    }
} // namespace native
