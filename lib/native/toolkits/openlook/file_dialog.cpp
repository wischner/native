//
// Adapts the standard XView File_chooser to portable owner-modal open
// and save dialogs, including native folder navigation and overwrite.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <filesystem>
#include <stdexcept>
#include <string>

#include <native.h>
#include <native/file_dialog.h>
#include <native/save_file_dialog.h>

#include <xview/file_chsr.h>
#include <xview/frame.h>
#include <xview/panel.h>
#include <xview/window.h>
#include <xview/xview.h>

#include "globals.h"
#include "window_position.h"

namespace
{
    std::string add_default_extension(
        const std::string &path,
        const std::string &extension) {
        if (path.empty() || extension.empty())
            return path;
        std::filesystem::path result(path);
        if (result.has_extension())
            return path;
        result += extension.front() == '.' ? extension
                                           : "." + extension;
        return result.string();
    }

    std::string regex_from_filters(
        const std::vector<native::file_filter> &filters) {
        std::string result;
        for (const native::file_filter &filter : filters) {
            for (const std::string &pattern : filter.patterns) {
                if (!result.empty())
                    result += "\\|";
                result.push_back('^');
                for (char value : pattern) {
                    switch (value) {
                    case '*':
                        result += ".*";
                        break;
                    case '?':
                        result.push_back('.');
                        break;
                    case '.':
                    case '^':
                    case '$':
                    case '[':
                    case ']':
                    case '\\':
                        result.push_back('\\');
                        result.push_back(value);
                        break;
                    default:
                        result.push_back(value);
                        break;
                    }
                }
                result.push_back('$');
            }
        }
        return result.empty() ? ".*" : result;
    }

    void release_dialog(native::file_dialog *dialog) {
        if (!dialog)
            return;
        auto *state = linux::openlook::file_dialog_bindings
                          .object_from_handle(dialog);
        if (!state)
            return;

        linux::openlook::file_dialog_bindings
            .unregister_by_handle(dialog);
        File_chooser chooser = state->chooser;
        state->chooser = XV_NULL;
        delete state;
        if (chooser)
            xv_destroy_safe(chooser);
    }

    int open_accepted(File_chooser chooser,
                      char *path,
                      char *,
                      Xv_opaque) {
        auto *dialog = reinterpret_cast<native::file_dialog *>(
            xv_get(chooser, WIN_CLIENT_DATA));
        if (!dialog || !path)
            return XV_ERROR;
        const std::string result(path);
        release_dialog(dialog);
        dialog->on_native_accept({result});
        return XV_OK;
    }

    int save_accepted(File_chooser chooser,
                      char *path,
                      struct stat *) {
        auto *dialog = reinterpret_cast<native::file_dialog *>(
            xv_get(chooser, WIN_CLIENT_DATA));
        if (!dialog || !path)
            return XV_ERROR;
        std::string result(path);
        auto *save = dynamic_cast<native::save_file_dialog *>(dialog);
        if (save) {
            result = add_default_extension(
                result, save->get_default_extension());
        }
        release_dialog(dialog);
        dialog->on_native_accept({result});
        return XV_OK;
    }

    void cancel(Panel_item item, Event *) {
        auto *dialog = reinterpret_cast<native::file_dialog *>(
            xv_get(item, PANEL_CLIENT_DATA));
        if (!dialog)
            return;
        release_dialog(dialog);
        dialog->on_native_cancel();
    }

    void close_frame(Frame frame) {
        auto *dialog = reinterpret_cast<native::file_dialog *>(
            xv_get(frame, WIN_CLIENT_DATA));
        if (!dialog)
            return;
        release_dialog(dialog);
        dialog->on_native_cancel();
    }
} // namespace

namespace linux::openlook
{
    void show_file_dialog(native::file_dialog &dialog,
                          bool save,
                          bool directory) {
        native::app_wnd *owner = dialog.get_owner();
        auto *owner_state = owner ? window_state(owner) : nullptr;
        if (!owner_state || !owner_state->frame) {
            throw std::runtime_error(
                "OpenLook/XView: file dialog has no owner frame.");
        }

        const std::string initial_directory =
            dialog.get_initial_path().empty()
                ? "."
                : dialog.get_initial_path().string();
        const char *document = "";
        bool confirm_overwrite = true;
        if (auto *save_dialog =
                dynamic_cast<native::save_file_dialog *>(&dialog)) {
            document = save_dialog->get_suggested_name().c_str();
            confirm_overwrite =
                save_dialog->get_confirm_overwrite();
        }
        const std::string filter = regex_from_filters(
            dialog.get_filters());
        const int filter_mask = dialog.get_filters().empty()
                                    ? FC_ALL_MASK
                                    : FC_MATCHED_FILES_MASK |
                                          FC_MATCHED_DIRS_MASK |
                                          FC_NOT_MATCHED_DIRS_MASK |
                                          FC_DOTDOT_MASK;

        File_chooser chooser = static_cast<File_chooser>(xv_create(
            owner_state->frame,
            FILE_CHOOSER,
            FILE_CHOOSER_TYPE,
            save ? FILE_CHOOSER_SAVEAS
                 : directory ? FILE_CHOOSER_SAVE
                             : FILE_CHOOSER_OPEN,
            FRAME_LABEL,
            dialog.get_title().c_str(),
            FRAME_DONE_PROC,
            close_frame,
            WIN_CLIENT_DATA,
            &dialog,
            FILE_CHOOSER_DIRECTORY,
            initial_directory.c_str(),
            FILE_CHOOSER_FILTER_STRING,
            filter.c_str(),
            FILE_CHOOSER_FILTER_MASK,
            filter_mask,
            FILE_CHOOSER_DOC_NAME,
            document,
            FILE_CHOOSER_NO_CONFIRM,
            save && !confirm_overwrite,
            FILE_CHOOSER_SAVE_TO_DIR,
            directory,
            nullptr));
        if (!chooser) {
            throw std::runtime_error(
                "OpenLook/XView: failed to create file chooser.");
        }

        auto *state = new openlook_file_dialog;
        state->chooser = chooser;
        state->dialog = &dialog;
        state->save = save || directory;
        if (save || directory) {
            xv_set(chooser,
                   FILE_CHOOSER_NOTIFY_FUNC,
                   save_accepted,
                   nullptr);
        } else {
            xv_set(chooser,
                   FILE_CHOOSER_NOTIFY_FUNC,
                   open_accepted,
                   nullptr);
        }
        try {
            file_dialog_bindings.register_pair(&dialog, state);
        } catch (...) {
            xv_destroy_safe(chooser);
            delete state;
            throw;
        }

        Panel_item cancel_button = static_cast<Panel_item>(xv_get(
            chooser,
            FILE_CHOOSER_CHILD,
            FILE_CHOOSER_CANCEL_BUTTON_CHILD));
        if (cancel_button) {
            xv_set(cancel_button,
                   PANEL_CLIENT_DATA,
                   &dialog,
                   PANEL_NOTIFY_PROC,
                   cancel,
                   nullptr);
        }
        const native::size dimensions(
            static_cast<native::dim>(xv_get(chooser, XV_WIDTH)),
            static_cast<native::dim>(xv_get(chooser, XV_HEIGHT)));
        const native::point owner_position = owner->get_position();
        const native::size owner_dimensions = owner->get_dimensions();
        const native::point preferred(
            owner_position.x +
                (owner_dimensions.w - dimensions.w) / 2,
            owner_position.y +
                (owner_dimensions.h - dimensions.h) / 2);
        const native::point position = constrain_frame_position(
            chooser, preferred, dimensions);
        xv_set(chooser,
               XV_X,
               position.x,
               XV_Y,
               position.y,
               XV_SHOW,
               TRUE,
               WIN_FRONT,
               WIN_SET_FOCUS,
               nullptr);
    }
} // namespace linux::openlook

namespace native
{
    void file_dialog::cancel_native_dialog() {
        release_dialog(this);
    }
} // namespace native
