//
// Implements the standard Haiku BFilePanel adapter, including native
// message dispatch, filename filtering, and safe asynchronous cleanup.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "file_dialog_common.h"

#include <algorithm>
#include <cctype>
#include <fnmatch.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <Application.h>
#include <Entry.h>
#include <FilePanel.h>
#include <Handler.h>
#include <Message.h>
#include <Messenger.h>
#include <Path.h>
#include <Window.h>

#include <native/file_dialog.h>

#include "globals.h"

namespace
{
    constexpr const char *dialog_field = "native_dialog";
    constexpr const char *state_field = "native_dialog_state";

    // Release resources held by an unregistered dialog state.
    struct file_dialog_state_deleter
    {
        void operator()(haiku::haiku_file_dialog *state) const {
            if (!state)
                return;
            delete state->panel;
            delete state->filter;
            delete state;
        }
    };

    using owned_file_dialog_state =
        std::unique_ptr<haiku::haiku_file_dialog,
                        file_dialog_state_deleter>;

    // Append a configured extension only when the leaf has none.
    std::string add_default_extension(
        const std::string &path, const std::string &extension) {
        if (path.empty() || extension.empty())
            return path;

        const std::size_t slash = path.find_last_of("/\\");
        const std::size_t dot = path.find_last_of('.');
        if (dot != std::string::npos &&
            (slash == std::string::npos || dot > slash + 1))
            return path;

        std::string result = path;
        if (extension.front() != '.')
            result.push_back('.');
        result += extension;
        return result;
    }

    // Admit directories and files matching any portable filter pattern.
    class pattern_filter final : public BRefFilter
    {
    public:
        explicit pattern_filter(
            const std::vector<native::file_filter> &filters) {
            for (const native::file_filter &filter : filters) {
                patterns_.insert(patterns_.end(),
                                 filter.patterns.begin(),
                                 filter.patterns.end());
            }
        }

        bool Filter(const entry_ref *reference,
                    BNode *,
                    struct stat_beos *,
                    const char *) override {
            if (!reference)
                return false;

            BEntry entry(reference, true);
            if (entry.IsDirectory())
                return true;

            std::string name = reference->name;
            std::transform(name.begin(),
                           name.end(),
                           name.begin(),
                           [](unsigned char value) {
                               return static_cast<char>(
                                   std::tolower(value));
                           });
            for (const std::string &pattern : patterns_) {
                std::string folded_pattern = pattern;
                std::transform(
                    folded_pattern.begin(),
                    folded_pattern.end(),
                    folded_pattern.begin(),
                    [](unsigned char value) {
                        return static_cast<char>(
                            std::tolower(value));
                    });
                if (fnmatch(folded_pattern.c_str(),
                            name.c_str(),
                            0) == 0) {
                    return true;
                }
            }
            return patterns_.empty();
        }

    private:
        std::vector<std::string> patterns_;
    };

    // Route panel messages to the registered portable dialog.
    class file_dialog_handler final : public BHandler
    {
    public:
        file_dialog_handler()
            : BHandler("native file dialog handler") {}

        void MessageReceived(BMessage *message) override {
            void *dialog_pointer = nullptr;
            void *state_pointer = nullptr;
            if (!message ||
                message->FindPointer(dialog_field,
                                     &dialog_pointer) != B_OK ||
                message->FindPointer(state_field,
                                     &state_pointer) != B_OK) {
                BHandler::MessageReceived(message);
                return;
            }

            auto *dialog =
                static_cast<native::file_dialog *>(dialog_pointer);
            auto *state =
                static_cast<haiku::haiku_file_dialog *>(state_pointer);
            if (haiku::file_dialog_bindings.object_from_handle(
                    dialog) != state)
                return;

            if (message->what == B_CANCEL) {
                dialog->on_native_cancel();
                return;
            }

            std::vector<std::string> paths;
            if (message->what == B_REFS_RECEIVED) {
                for (int32 index = 0;; ++index) {
                    entry_ref reference;
                    if (message->FindRef(
                            "refs", index, &reference) != B_OK)
                        break;
                    BPath path(&reference);
                    if (path.InitCheck() == B_OK)
                        paths.emplace_back(path.Path());
                }
            } else if (message->what == B_SAVE_REQUESTED) {
                entry_ref directory;
                const char *name = nullptr;
                if (message->FindRef("directory", &directory) == B_OK &&
                    message->FindString("name", &name) == B_OK &&
                    name) {
                    BPath path(&directory);
                    if (path.InitCheck() == B_OK &&
                        path.Append(name) == B_OK) {
                        paths.push_back(add_default_extension(
                            path.Path(), state->default_extension));
                    }
                }
            }

            if (paths.empty())
                dialog->on_native_cancel();
            else
                dialog->on_native_accept(paths);
        }
    };

    // One application-owned handler receives all file-panel messages.
    file_dialog_handler dialog_handler;

    // Attach the shared message handler to the active BApplication.
    void ensure_handler() {
        if (dialog_handler.Looper() == haiku::global_app)
            return;
        if (!haiku::global_app)
            throw std::runtime_error(
                "Haiku: A file panel requires BApplication.");

        if (BLooper *old_looper = dialog_handler.Looper()) {
            if (!old_looper->Lock())
                throw std::runtime_error(
                    "Haiku: Unable to lock the old file-panel "
                    "message looper.");
            old_looper->RemoveHandler(&dialog_handler);
            old_looper->Unlock();
        }

        if (!haiku::global_app->Lock())
            throw std::runtime_error(
                "Haiku: Unable to lock BApplication for a file "
                "panel.");
        haiku::global_app->AddHandler(&dialog_handler);
        haiku::global_app->Unlock();
    }

    // Set the panel directory from either a folder or file path.
    void set_initial_directory(
        BFilePanel *panel, const std::string &path) {
        if (!panel || path.empty())
            return;

        BEntry entry(path.c_str(), true);
        if (entry.InitCheck() != B_OK)
            return;
        if (entry.IsDirectory()) {
            panel->SetPanelDirectory(&entry);
            return;
        }

        BEntry parent;
        if (entry.GetParent(&parent) == B_OK)
            panel->SetPanelDirectory(&parent);
    }
} // namespace

namespace haiku
{
    void show_file_dialog(native::file_dialog &dialog,
                          bool save,
                          bool allow_multiple,
                          const std::string &suggested_name,
                          const std::string &default_extension) {
        ensure_handler();

        owned_file_dialog_state state(new haiku_file_dialog);
        state->default_extension = default_extension;
        if (!dialog.get_filters().empty())
            state->filter = new pattern_filter(dialog.get_filters());

        const uint32 message_code = save
                                        ? static_cast<uint32>(
                                              B_SAVE_REQUESTED)
                                        : static_cast<uint32>(
                                              B_REFS_RECEIVED);
        BMessage message(message_code);
        message.AddPointer(dialog_field, &dialog);
        message.AddPointer(state_field, state.get());
        BMessenger target(&dialog_handler);
        state->panel = new BFilePanel(
            save ? B_SAVE_PANEL : B_OPEN_PANEL,
            &target,
            nullptr,
            B_FILE_NODE | B_SYMLINK_NODE,
            allow_multiple,
            &message,
            state->filter,
            false,
            true);
        set_initial_directory(state->panel,
                              dialog.get_initial_path());
        if (save && !suggested_name.empty())
            state->panel->SetSaveText(suggested_name.c_str());

        BWindow *panel_window = state->panel->Window();
        BWindow *owner_window = dialog.get_owner()
                                    ? wnd_bindings.handle_from_object(
                                          dialog.get_owner())
                                    : nullptr;
        if (panel_window && panel_window->Lock()) {
            panel_window->SetTitle(dialog.get_title().c_str());
            if (owner_window) {
                panel_window->SetLook(B_MODAL_WINDOW_LOOK);
                panel_window->SetFeel(B_MODAL_SUBSET_WINDOW_FEEL);
                panel_window->AddToSubset(owner_window);
            }
            panel_window->Unlock();
        }

        file_dialog_bindings.register_pair(&dialog, state.get());
        BFilePanel *panel = state->panel;
        state.release();
        panel->Show();
        if (panel_window)
            panel_window->Activate(true);
    }
} // namespace haiku

namespace native
{
    void file_dialog::cancel_native_dialog() const {
        auto *self = const_cast<file_dialog *>(this);
        auto *state =
            haiku::file_dialog_bindings.object_from_handle(self);
        if (!state)
            return;

        haiku::file_dialog_bindings.unregister_by_handle(self);
        if (state->panel)
            state->panel->Hide();
        delete state->panel;
        delete state->filter;
        delete state;
    }
} // namespace native
