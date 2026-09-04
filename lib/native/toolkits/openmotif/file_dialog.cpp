//
// Implements the standard Motif FileSelectionBox adapter, callback
// translation, filename filtering, and native widget cleanup.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "file_dialog_common.h"

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include <X11/Intrinsic.h>
#include <Xm/FileSB.h>
#include <Xm/MessageB.h>
#include <Xm/Xm.h>

#include <native/file_dialog.h>
#include <native/save_file_dialog.h>

#include "globals.h"

namespace
{
    namespace fs = std::filesystem;

    // Append the configured suffix only to an extensionless path.
    std::string add_default_extension(
        const std::string &path, const std::string &extension) {
        if (path.empty() || extension.empty())
            return path;
        fs::path result(path);
        if (result.has_extension())
            return path;
        result += extension.front() == '.' ? extension
                                           : "." + extension;
        return result.string();
    }

    // Return whether the selected path names an existing file.
    bool file_exists(const std::string &path) {
        std::error_code error;
        return fs::is_regular_file(fs::path(path), error);
    }

    // Release every native widget before portable completion runs.
    void release_dialog(native::file_dialog *dialog) {
        if (!dialog)
            return;

        auto *state = linux::openmotif::file_dialog_bindings
                          .object_from_handle(dialog);
        if (!state)
            return;

        linux::openmotif::file_dialog_bindings.unregister_by_handle(
            dialog);
        Widget confirmation = state->confirmation;
        Widget selector = state->selector;
        state->confirmation = nullptr;
        state->selector = nullptr;
        if (confirmation)
            XtDestroyWidget(confirmation);
        if (selector)
            XtDestroyWidget(selector);
        delete state;
    }

    // Finish a confirmed save using the pending selected path.
    void handle_overwrite_accept(Widget,
                                 XtPointer client_data,
                                 XtPointer) {
        auto *dialog =
            static_cast<native::file_dialog *>(client_data);
        auto *state = dialog
                          ? linux::openmotif::file_dialog_bindings
                                .object_from_handle(dialog)
                          : nullptr;
        if (!state || state->pending_path.empty())
            return;

        const std::string path = state->pending_path;
        release_dialog(dialog);
        dialog->on_native_accept({path});
    }

    // Return from overwrite confirmation to the file selector.
    void handle_overwrite_cancel(Widget widget,
                                 XtPointer client_data,
                                 XtPointer) {
        auto *dialog =
            static_cast<native::file_dialog *>(client_data);
        auto *state = dialog
                          ? linux::openmotif::file_dialog_bindings
                                .object_from_handle(dialog)
                          : nullptr;
        if (!state || state->confirmation != widget)
            return;

        state->confirmation = nullptr;
        state->pending_path.clear();
        XtDestroyWidget(widget);
        if (state->selector)
            XtSetSensitive(state->selector, True);
    }

    // Re-enable the selector if the window manager closes the prompt.
    void handle_overwrite_destroy(Widget widget,
                                  XtPointer client_data,
                                  XtPointer) {
        auto *dialog =
            static_cast<native::file_dialog *>(client_data);
        auto *state = dialog
                          ? linux::openmotif::file_dialog_bindings
                                .object_from_handle(dialog)
                          : nullptr;
        if (!state || state->confirmation != widget)
            return;

        state->confirmation = nullptr;
        state->pending_path.clear();
        if (state->selector)
            XtSetSensitive(state->selector, True);
    }

    // Ask the user before replacing an existing file.
    void show_overwrite_confirmation(
        native::file_dialog *dialog,
        linux::openmotif::motif_file_dialog *state,
        const std::string &path) {
        if (!dialog || !state || !state->selector)
            return;

        XmString message = XmStringCreateLocalized(
            const_cast<char *>("The file exists. Replace it?"));
        XmString replace = XmStringCreateLocalized(
            const_cast<char *>("Replace"));
        XmString cancel = XmStringCreateLocalized(
            const_cast<char *>("Cancel"));
        Arg arguments[4];
        Cardinal count = 0;
        XtSetArg(arguments[count],
                 XmNdialogStyle,
                 XmDIALOG_PRIMARY_APPLICATION_MODAL);
        ++count;
        XtSetArg(arguments[count], XmNmessageString, message);
        ++count;
        XtSetArg(arguments[count], XmNokLabelString, replace);
        ++count;
        XtSetArg(arguments[count], XmNcancelLabelString, cancel);
        ++count;

        Widget confirmation = XmCreateQuestionDialog(
            state->selector,
            const_cast<char *>("overwrite_confirmation"),
            arguments,
            count);
        XmStringFree(cancel);
        XmStringFree(replace);
        XmStringFree(message);
        if (!confirmation)
            throw std::runtime_error(
                "Motif: Failed to create overwrite confirmation.");

        state->pending_path = path;
        state->confirmation = confirmation;
        Widget help = XmMessageBoxGetChild(
            confirmation, XmDIALOG_HELP_BUTTON);
        if (help)
            XtUnmanageChild(help);
        XtAddCallback(confirmation,
                      XmNokCallback,
                      handle_overwrite_accept,
                      dialog);
        XtAddCallback(confirmation,
                      XmNcancelCallback,
                      handle_overwrite_cancel,
                      dialog);
        XtAddCallback(confirmation,
                      XmNdestroyCallback,
                      handle_overwrite_destroy,
                      dialog);
        XtSetSensitive(state->selector, False);
        XtManageChild(confirmation);
    }

    // Translate the Motif selection callback into a portable result.
    void handle_accept(Widget widget,
                       XtPointer client_data,
                       XtPointer call_data) {
        auto *dialog =
            static_cast<native::file_dialog *>(client_data);
        auto *callback = static_cast<
            XmFileSelectionBoxCallbackStruct *>(call_data);
        auto *state = dialog
                          ? linux::openmotif::file_dialog_bindings
                                .object_from_handle(dialog)
                          : nullptr;
        if (!dialog || !callback || !state ||
            state->selector != widget)
            return;

        char *value = nullptr;
        if (!XmStringGetLtoR(callback->value,
                            XmFONTLIST_DEFAULT_TAG,
                            &value) ||
            !value) {
            release_dialog(dialog);
            dialog->on_native_cancel();
            return;
        }

        std::string path(value);
        XtFree(value);
        if (auto *save =
                dynamic_cast<native::save_file_dialog *>(dialog)) {
            path = add_default_extension(
                path, save->get_default_extension());
            if (save->get_confirm_overwrite() &&
                file_exists(path)) {
                try {
                    show_overwrite_confirmation(
                        dialog, state, path);
                } catch (...) {
                    release_dialog(dialog);
                    dialog->on_native_cancel();
                }
                return;
            }
        }

        release_dialog(dialog);
        dialog->on_native_accept({path});
    }

    // Translate the Motif cancel callback into a portable result.
    void handle_cancel(Widget widget,
                       XtPointer client_data,
                       XtPointer) {
        auto *dialog =
            static_cast<native::file_dialog *>(client_data);
        auto *state = dialog
                          ? linux::openmotif::file_dialog_bindings
                                .object_from_handle(dialog)
                          : nullptr;
        if (!dialog || !state || state->selector != widget)
            return;

        release_dialog(dialog);
        dialog->on_native_cancel();
    }

    // Treat unexpected selector destruction as cancellation.
    void handle_destroy(Widget widget,
                        XtPointer client_data,
                        XtPointer) {
        auto *dialog =
            static_cast<native::file_dialog *>(client_data);
        auto *state = dialog
                          ? linux::openmotif::file_dialog_bindings
                                .object_from_handle(dialog)
                          : nullptr;
        if (!dialog || !state || state->selector != widget)
            return;

        linux::openmotif::file_dialog_bindings.unregister_by_handle(
            dialog);
        state->selector = nullptr;
        Widget confirmation = state->confirmation;
        state->confirmation = nullptr;
        if (confirmation)
            XtDestroyWidget(confirmation);
        delete state;
        dialog->on_native_cancel();
    }

    // Build an inclusive initial Motif directory mask. Motif exposes
    // its filter field directly, so users can narrow this after the
    // selector opens; starting with only the first portable pattern
    // incorrectly hid every other filter group.
    std::string directory_mask(const native::file_dialog &dialog) {
        const fs::path initial(dialog.get_initial_path());
        return initial.empty() ? std::string("*")
                               : (initial / "*").string();
    }
} // namespace

namespace linux::openmotif
{
    void show_file_dialog(native::file_dialog &dialog,
                          bool save,
                          bool directory) {
        native::app_wnd *owner = dialog.get_owner();
        Widget owner_shell =
            owner ? shell_bindings.handle_from_object(owner)
                  : nullptr;
        if (!owner_shell)
            throw std::runtime_error(
                "Motif: Missing owner shell for a file dialog.");

        Arg arguments[8];
        Cardinal count = 0;
        XtSetArg(arguments[count],
                 XmNdialogStyle,
                 XmDIALOG_PRIMARY_APPLICATION_MODAL);
        ++count;
        XtSetArg(arguments[count], XmNmustMatch, save ? False : True);
        ++count;
        XtSetArg(arguments[count], XmNautoUnmanage, False);
        ++count;
        if (directory) {
            XtSetArg(arguments[count],
                     XmNfileTypeMask,
                     XmFILE_DIRECTORY);
            ++count;
        }

        XmString title =
            XmStringCreateLocalized(
                const_cast<char *>(dialog.get_title().c_str()));
        XtSetArg(arguments[count], XmNdialogTitle, title);
        ++count;

        const std::string mask_text = directory_mask(dialog);
        XmString mask = XmStringCreateLocalized(
            const_cast<char *>(mask_text.c_str()));
        XtSetArg(arguments[count], XmNdirMask, mask);
        ++count;

        Widget widget = XmCreateFileSelectionDialog(
            owner_shell,
            const_cast<char *>(save ? "save_file_dialog"
                              : directory ? "directory_dialog"
                                          : "open_file_dialog"),
            arguments,
            count);
        XmStringFree(mask);
        XmStringFree(title);
        if (!widget)
            throw std::runtime_error(
                "Motif: Failed to create a file dialog.");

        if (save) {
            auto *save_dialog =
                dynamic_cast<native::save_file_dialog *>(&dialog);
            if (save_dialog &&
                !save_dialog->get_suggested_name().empty()) {
                XmString name = XmStringCreateLocalized(
                    const_cast<char *>(save_dialog
                                           ->get_suggested_name()
                                           .c_str()));
                XtVaSetValues(widget, XmNtextString, name, nullptr);
                XmStringFree(name);
            }
        }

        auto *state = new motif_file_dialog;
        state->selector = widget;
        try {
            file_dialog_bindings.register_pair(&dialog, state);
        } catch (...) {
            XtDestroyWidget(widget);
            delete state;
            throw;
        }
        XtAddCallback(widget,
                      XmNokCallback,
                      handle_accept,
                      &dialog);
        XtAddCallback(widget,
                      XmNcancelCallback,
                      handle_cancel,
                      &dialog);
        XtAddCallback(widget,
                      XmNdestroyCallback,
                      handle_destroy,
                      &dialog);
        XtManageChild(widget);
    }
} // namespace linux::openmotif

namespace native
{
    void file_dialog::cancel_native_dialog() {
        auto *self = this;
        release_dialog(self);
    }
} // namespace native
