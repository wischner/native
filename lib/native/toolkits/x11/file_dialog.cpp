//
// Implements the native Athena fallback used when a Linux desktop
// chooser is not installed. Every visible control is an Xaw widget.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/file_dialog.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fnmatch.h>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <X11/Intrinsic.h>
#include <X11/Shell.h>
#include <X11/StringDefs.h>
#include <X11/Xaw/AsciiText.h>
#include <X11/Xaw/Command.h>
#include <X11/Xaw/Form.h>
#include <X11/Xaw/Label.h>
#include <X11/Xaw/List.h>
#include <X11/Xaw/Viewport.h>

#include <native/open_file_dialog.h>
#include <native/save_file_dialog.h>
#include <bindings.h>

#include "../../platforms/linux/file_dialog_process.h"
#include "file_dialog_fallback.h"
#include "globals.h"

namespace
{
    namespace fs = std::filesystem;
    using chooser_state = linux::x11::xaw_file_dialog;

    // Determine whether a leaf name matches any configured filter.
    bool matches_filters(const native::file_dialog &dialog,
                         const std::string &name) {
        const auto &filters = dialog.get_filters();
        if (filters.empty())
            return true;
        std::string folded_name = name;
        std::transform(
            folded_name.begin(),
            folded_name.end(),
            folded_name.begin(),
            [](unsigned char value) {
                return static_cast<char>(std::tolower(value));
            });
        for (const native::file_filter &filter : filters) {
            for (const std::string &pattern : filter.patterns) {
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
                            folded_name.c_str(),
                            0) == 0) {
                    return true;
                }
            }
        }
        return false;
    }

    // Replace the path editor contents with a copied UTF-8 path.
    void set_path_text(chooser_state &state, const fs::path &path) {
        const std::string text = path.string();
        XtVaSetValues(state.path_edit,
                      XtNstring,
                      text.c_str(),
                      nullptr);
        state.pending_overwrite.clear();
    }

    // Report browser state or a validation error above the list.
    void set_status(chooser_state &state, const std::string &text) {
        XtVaSetValues(state.directory_label,
                      XtNlabel,
                      text.c_str(),
                      nullptr);
    }

    void populate(chooser_state &state);

    // Enter a directory without ending the active modal session.
    void enter_directory(chooser_state &state,
                         const fs::path &directory) {
        state.directory = directory;
        state.last_selection = -1;
        state.last_selection_time = 0;
        populate(state);
        set_path_text(
            state,
            state.save && !state.suggested_name.empty()
                ? state.directory / state.suggested_name
                : state.directory);
    }

    // Rebuild the directory listing while retaining stable Xaw strings.
    void populate(chooser_state &state) {
        struct item
        {
            std::string label;
            fs::path path;
            bool directory = false;
        };

        std::vector<item> items;
        const fs::path parent = state.directory.parent_path();
        if (!parent.empty() && parent != state.directory)
            items.push_back({"[..]", parent, true});

        std::error_code error;
        fs::directory_iterator iterator(state.directory, error);
        const fs::directory_iterator end;
        while (!error && iterator != end) {
            const fs::directory_entry entry = *iterator;
            iterator.increment(error);
            const std::string leaf = entry.path().filename().string();
            std::error_code type_error;
            const bool is_directory = entry.is_directory(type_error);
            if (type_error)
                continue;
            if (!is_directory &&
                !matches_filters(*state.dialog, leaf)) {
                continue;
            }
            items.push_back({is_directory ? "[" + leaf + "]" : leaf,
                             entry.path(),
                             is_directory});
        }

        std::sort(items.begin(), items.end(),
                  [](const item &left, const item &right) {
                      if (left.directory != right.directory)
                          return left.directory > right.directory;
                      return left.label < right.label;
                  });

        state.labels.clear();
        state.paths.clear();
        state.labels.reserve(items.size());
        state.paths.reserve(items.size());
        for (item &entry : items) {
            state.labels.push_back(std::move(entry.label));
            state.paths.push_back(std::move(entry.path));
        }
        state.label_pointers.clear();
        state.label_pointers.reserve(state.labels.size());
        for (std::string &label : state.labels)
            state.label_pointers.push_back(label.data());

        XawListChange(
            state.list,
            state.label_pointers.empty()
                ? nullptr
                : state.label_pointers.data(),
            static_cast<int>(state.label_pointers.size()),
            0,
            True);
        set_status(state, state.directory.string());
    }

    // Return the complete path currently entered by the user.
    fs::path entered_path(const chooser_state &state) {
        String value = nullptr;
        XtVaGetValues(state.path_edit, XtNstring, &value, nullptr);
        return value ? fs::path(value) : fs::path();
    }

    // Select a listed entry and copy its path into the editor.
    void on_list_selection(Widget,
                           XtPointer client_data,
                           XtPointer call_data) {
        auto *state = static_cast<chooser_state *>(client_data);
        auto *selection =
            static_cast<XawListReturnStruct *>(call_data);
        if (!state || !selection || selection->list_index < 0)
            return;
        const std::size_t index =
            static_cast<std::size_t>(selection->list_index);
        if (index >= state->paths.size())
            return;

        const fs::path path = state->paths[index];
        set_path_text(*state, path);

        Display *display = XtDisplay(state->list);
        const Time event_time = XtLastTimestampProcessed(display);
        const Time elapsed = event_time - state->last_selection_time;
        const bool double_click =
            state->last_selection == selection->list_index &&
            state->last_selection_time != 0 &&
            elapsed <= static_cast<Time>(
                           XtGetMultiClickTime(display));
        state->last_selection = selection->list_index;
        state->last_selection_time = event_time;

        std::error_code error;
        if (double_click && fs::is_directory(path, error) && !error)
            enter_directory(*state, path);
    }

    // Navigate upward without ending the active modal session.
    void on_up(Widget, XtPointer client_data, XtPointer) {
        auto *state = static_cast<chooser_state *>(client_data);
        if (!state)
            return;
        const fs::path parent = state->directory.parent_path();
        if (parent.empty() || parent == state->directory)
            return;
        enter_directory(*state, parent);
    }

    // Accept a file or navigate into an entered directory.
    void on_accept(Widget, XtPointer client_data, XtPointer) {
        auto *state = static_cast<chooser_state *>(client_data);
        if (!state || !state->dialog)
            return;

        fs::path path = entered_path(*state);
        if (path.empty()) {
            set_status(*state, "Enter or select a path.");
            return;
        }
        if (path.is_relative())
            path = state->directory / path;
        path = path.lexically_normal();

        std::error_code error;
        if (fs::is_directory(path, error) && !error) {
            enter_directory(*state, path);
            return;
        }

        if (!state->save) {
            if (!fs::is_regular_file(path, error) || error) {
                set_status(*state, "The selected file does not exist.");
                return;
            }
        } else {
            path = ::linux::add_default_extension(
                path.string(), state->default_extension);
            const fs::path parent = path.parent_path().empty()
                                        ? state->directory
                                        : path.parent_path();
            if (!fs::is_directory(parent, error) || error) {
                set_status(*state,
                           "The destination folder does not exist.");
                return;
            }
            if (state->confirm_overwrite && fs::exists(path, error) &&
                !error && state->pending_overwrite != path.string()) {
                state->pending_overwrite = path.string();
                set_status(*state,
                           "File exists; choose Save again to replace "
                           "it.");
                set_path_text(*state, path);
                state->pending_overwrite = path.string();
                return;
            }
        }

        native::file_dialog *dialog = state->dialog;
        dialog->on_native_accept({path.string()});
    }

    // Cancel from an Athena command or the window-manager close button.
    void on_cancel(Widget, XtPointer client_data, XtPointer) {
        auto *state = static_cast<chooser_state *>(client_data);
        if (state && state->dialog)
            state->dialog->on_native_cancel();
    }

    // Translate the shell's WM_DELETE_WINDOW protocol into
    // cancellation.
    void on_shell_event(Widget,
                        XtPointer client_data,
                        XEvent *event,
                        Boolean *) {
        auto *state = static_cast<chooser_state *>(client_data);
        if (!state || !event || event->type != ClientMessage)
            return;
        const Atom protocol = XInternAtom(
            event->xclient.display, "WM_DELETE_WINDOW", False);
        if (event->xclient.data.l[0] == static_cast<long>(protocol))
            on_cancel(nullptr, state, nullptr);
    }
} // namespace

namespace native
{
    void file_dialog::cancel_native_dialog() const {
        chooser_state *state = linux::x11::file_dialog_bindings
                                   .object_from_handle(this);
        if (!state)
            return;

        linux::x11::file_dialog_bindings.unregister_by_handle(this);
        delete state;
    }
} // namespace native

namespace linux::x11
{
    bool show_file_dialog_fallback(
        native::file_dialog &dialog,
        bool save,
        const std::string &suggested_name,
        const std::string &default_extension,
        bool confirm_overwrite) {
        native::app_wnd *owner = dialog.get_owner();
        Widget owner_shell = owner
                                 ? shell_bindings.handle_from_object(
                                       owner)
                                 : nullptr;
        if (!owner_shell)
            return false;

        auto state = std::make_unique<chooser_state>();
        state->dialog = &dialog;
        state->save = save;
        state->suggested_name = suggested_name;
        state->default_extension = default_extension;
        state->confirm_overwrite = confirm_overwrite;

        std::error_code error;
        fs::path initial = dialog.get_initial_path();
        if (initial.empty())
            initial = fs::current_path(error);
        if (error)
            initial = ".";
        if (!fs::is_directory(initial, error) || error) {
            const fs::path parent = initial.parent_path();
            state->directory = parent.empty() ? fs::path(".") : parent;
        } else {
            state->directory = initial;
        }
        state->directory = fs::absolute(state->directory, error);
        if (error)
            state->directory = ".";

        state->shell = XtVaCreatePopupShell(
            "file_dialog",
            transientShellWidgetClass,
            owner_shell,
            XtNtitle,
            dialog.get_title().c_str(),
            XtNtransientFor,
            owner_shell,
            XtNwidth,
            560,
            XtNheight,
            410,
            nullptr);
        if (!state->shell) {
            return false;
        }

        Widget form = XtVaCreateManagedWidget(
            "file_form",
            formWidgetClass,
            state->shell,
            XtNdefaultDistance,
            8,
            nullptr);
        state->directory_label = XtVaCreateManagedWidget(
            "directory",
            labelWidgetClass,
            form,
            XtNlabel,
            state->directory.string().c_str(),
            XtNborderWidth,
            0,
            XtNwidth,
            536,
            nullptr);
        Widget viewport = XtVaCreateManagedWidget(
            "file_viewport",
            viewportWidgetClass,
            form,
            XtNfromVert,
            state->directory_label,
            XtNwidth,
            536,
            XtNheight,
            276,
            XtNallowVert,
            True,
            XtNallowHoriz,
            True,
            nullptr);
        state->list = XtVaCreateManagedWidget(
            "files",
            listWidgetClass,
            viewport,
            XtNwidth,
            516,
            XtNdefaultColumns,
            1,
            XtNforceColumns,
            True,
            XtNverticalList,
            True,
            nullptr);
        state->path_edit = XtVaCreateManagedWidget(
            "path",
            asciiTextWidgetClass,
            form,
            XtNfromVert,
            viewport,
            XtNeditType,
            XawtextEdit,
            XtNwidth,
            536,
            XtNheight,
            28,
            nullptr);
        Widget up = XtVaCreateManagedWidget(
            "up",
            commandWidgetClass,
            form,
            XtNfromVert,
            state->path_edit,
            XtNlabel,
            "Up",
            XtNwidth,
            88,
            nullptr);
        Widget accept = XtVaCreateManagedWidget(
            save ? "save" : "open",
            commandWidgetClass,
            form,
            XtNfromVert,
            state->path_edit,
            XtNfromHoriz,
            up,
            XtNlabel,
            save ? "Save" : "Open",
            XtNwidth,
            88,
            nullptr);
        Widget cancel = XtVaCreateManagedWidget(
            "cancel",
            commandWidgetClass,
            form,
            XtNfromVert,
            state->path_edit,
            XtNfromHoriz,
            accept,
            XtNlabel,
            "Cancel",
            XtNwidth,
            88,
            nullptr);

        XtAddCallback(state->list,
                      XtNcallback,
                      on_list_selection,
                      state.get());
        XtAddCallback(up, XtNcallback, on_up, state.get());
        XtAddCallback(accept, XtNcallback, on_accept, state.get());
        XtAddCallback(cancel, XtNcallback, on_cancel, state.get());
        XtAddEventHandler(state->shell,
                          NoEventMask,
                          True,
                          on_shell_event,
                          state.get());
        populate(*state);
        set_path_text(
            *state,
            save && !suggested_name.empty()
                ? state->directory / suggested_name
                : state->directory);

        XtRealizeWidget(state->shell);
        Atom protocol = XInternAtom(
            XtDisplay(state->shell), "WM_DELETE_WINDOW", False);
        XSetWMProtocols(XtDisplay(state->shell),
                        XtWindow(state->shell),
                        &protocol,
                        1);
        XtSetKeyboardFocus(form, state->path_edit);
        XtPopup(state->shell, XtGrabExclusive);
        file_dialog_bindings.register_pair(&dialog, state.get());
        state.release();
        return true;
    }
} // namespace linux::x11
