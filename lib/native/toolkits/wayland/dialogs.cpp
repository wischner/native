//
// Uses GTK 4.14 asynchronous file and alert dialogs with cancellable
// peers.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "../../message_box_common.h"
#include <memory>
namespace
{
    struct panel_state
    {
        GCancellable *cancel = g_cancellable_new();
        ~panel_state() {
            g_cancellable_cancel(cancel);
            g_object_unref(cancel);
        }
    };
    struct response_state
    {
        native::file_dialog *owner;
        native::detail::window_guard guard;
        GtkFileChooserAction action;
        bool multiple = false;
        response_state(native::file_dialog &owner,
                       GtkFileChooserAction action)
            : owner(&owner)
            , guard(owner)
            , action(action) {}
    };
    void
    selected(GObject *object, GAsyncResult *result, gpointer data) {
        std::unique_ptr<response_state> state(
            static_cast<response_state *>(data));
        GError *error = nullptr;
        auto *dialog = GTK_FILE_DIALOG(object);
        std::vector<std::filesystem::path> paths;
        const auto append = [&paths](GFile *file) {
            char *path = file ? g_file_get_path(file) : nullptr;
            if (path)
                paths.emplace_back(path);
            g_free(path);
        };
        if (state->multiple) {
            auto *files = gtk_file_dialog_open_multiple_finish(
                dialog, result, &error);
            if (files) {
                for (guint i = 0; i < g_list_model_get_n_items(files);
                     ++i) {
                    auto *file =
                        G_FILE(g_list_model_get_item(files, i));
                    append(file);
                    g_object_unref(file);
                }
                g_object_unref(files);
            }
        } else {
            GFile *file =
                state->action == GTK_FILE_CHOOSER_ACTION_OPEN
                    ? gtk_file_dialog_open_finish(
                          dialog, result, &error)
                : state->action == GTK_FILE_CHOOSER_ACTION_SAVE
                    ? gtk_file_dialog_save_finish(
                          dialog, result, &error)
                    : gtk_file_dialog_select_folder_finish(
                          dialog, result, &error);
            append(file);
            if (file)
                g_object_unref(file);
        }
        if (error)
            g_error_free(error);
        if (!state->guard.alive())
            return;
        if (auto *save =
                dynamic_cast<native::save_file_dialog *>(state->owner);
            save && !paths.empty() && !paths.front().has_extension() &&
            !save->get_default_extension().empty()) {
            auto extension = save->get_default_extension();
            if (extension.front() != '.')
                extension.insert(extension.begin(), '.');
            paths.front().replace_extension(extension);
            std::error_code error;
            if (save->get_confirm_overwrite() &&
                std::filesystem::exists(paths.front(), error)) {
                const auto answer = native::message_box::show(
                    *save->get_owner(),
                    "Replace the existing file?",
                    "Save File",
                    native::message_box_buttons::yes_no,
                    native::message_box_icon::question);
                if (!state->guard.alive())
                    return;
                if (answer != native::message_box_result::yes)
                    paths.clear();
            }
        }
        if (paths.empty())
            state->owner->on_native_cancel();
        else
            state->owner->on_native_accept(paths);
    }
    void choose(native::file_dialog &owner,
                GtkFileChooserAction action,
                const std::string &name = {}) {
        auto *parent = GTK_WINDOW(
            linux::wayland::state(*owner.get_owner()).widget);
        auto *panel = new panel_state;
        native::detail::assign_peer_state(owner, panel);
        auto *dialog = gtk_file_dialog_new();
        gtk_file_dialog_set_title(dialog, owner.get_title().c_str());
        if (!owner.get_initial_path().empty()) {
            auto *folder =
                g_file_new_for_path(owner.get_initial_path().c_str());
            std::error_code error;
            if (std::filesystem::is_directory(owner.get_initial_path(),
                                              error))
                gtk_file_dialog_set_initial_folder(dialog, folder);
            else
                gtk_file_dialog_set_initial_file(dialog, folder);
            g_object_unref(folder);
        }
        if (!name.empty())
            gtk_file_dialog_set_initial_name(dialog, name.c_str());
        auto *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
        for (const auto &filter : owner.get_filters()) {
            auto *native_filter = gtk_file_filter_new();
            gtk_file_filter_set_name(native_filter,
                                     filter.name.c_str());
            for (const auto &pattern : filter.patterns)
                gtk_file_filter_add_pattern(native_filter,
                                            pattern.c_str());
            g_list_store_append(filters, native_filter);
            g_object_unref(native_filter);
        }
        gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
        g_object_unref(filters);
        auto *response = new response_state(owner, action);
        if (auto *open =
                dynamic_cast<native::open_file_dialog *>(&owner))
            response->multiple = open->get_allow_multiple();
        if (response->multiple)
            gtk_file_dialog_open_multiple(
                dialog, parent, panel->cancel, selected, response);
        else if (action == GTK_FILE_CHOOSER_ACTION_OPEN)
            gtk_file_dialog_open(
                dialog, parent, panel->cancel, selected, response);
        else if (action == GTK_FILE_CHOOSER_ACTION_SAVE)
            gtk_file_dialog_save(
                dialog, parent, panel->cancel, selected, response);
        else
            gtk_file_dialog_select_folder(
                dialog, parent, panel->cancel, selected, response);
        g_object_unref(dialog);
    }
} // namespace
namespace native
{
    void file_dialog::cancel_native_dialog() {
        if (auto *panel = detail::peer_state<panel_state>(*this))
            g_cancellable_cancel(panel->cancel);
    }
    void open_file_dialog::show_native() {
        if (begin_dialog())
            choose(*this, GTK_FILE_CHOOSER_ACTION_OPEN);
    }
    void save_file_dialog::show_native() {
        if (begin_dialog())
            choose(*this,
                   GTK_FILE_CHOOSER_ACTION_SAVE,
                   get_suggested_name());
    }
    void directory_dialog::show_native() {
        if (begin_dialog())
            choose(*this, GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    }
    message_box_result message_box::show(app_wnd &owner,
                                         const std::string &message,
                                         const std::string &title,
                                         message_box_buttons buttons,
                                         message_box_icon icon) {
        detail::validate_message_box_owner(owner);
        return linux::wayland::show_message(
            owner, message, title, buttons, icon);
    }
} // namespace native
