//
// Composes owner-centered GEM file choosers from native AES form
// controls. Directory enumeration and filename filtering use portable
// filesystem code.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "file_dialog_common.h"
#include "dialog_form.h"
#include "globals.h"
#include "../../file_filter_match.h"
#include <native/message_box.h>
#include <algorithm>
#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    struct file_item
    {
        fs::path path;
        bool directory;
    };

    // Copy a complete editable path without overrunning the AES buffer.
    void copy_path(std::array<char, 1024> &buffer,
                   const fs::path &path) {
        const auto text = path.string();
        buffer.fill(0);
        std::copy_n(text.begin(),
                    std::min(text.size(), buffer.size() - 1),
                    buffer.begin());
    }

    // Match all patterns in the selected portable filter group.
    bool matches(const native::file_dialog &dialog,
                 int filter,
                 const std::string &name) {
        if (dialog.get_filters().empty())
            return true;
        const auto &patterns = dialog.get_filters().at(filter).patterns;
        if (patterns.empty())
            return true;
        return std::any_of(
            patterns.begin(), patterns.end(), [&](const auto &pattern) {
                return native::detail::matches_file_pattern(pattern,
                                                            name);
            });
    }

    // Keep long labels inside the fixed native form; paths remain
    // intact.
    std::string caption(std::string value, std::size_t capacity) {
        if (value.size() > capacity)
            value = value.substr(0, capacity - 3) + "...";
        return value;
    }
} // namespace

namespace linux::gemix
{
    file_dialog_response
    show_file_dialog(native::file_dialog &dialog,
                     const std::string &suggested_name,
                     bool save,
                     bool directory_only,
                     const std::string &extension,
                     bool confirm_overwrite) {
        std::error_code error;
        fs::path initial = dialog.get_initial_path();
        if (initial.empty())
            initial = fs::current_path(error);
        fs::path directory = fs::is_directory(initial, error)
                                 ? initial
                                 : initial.parent_path();
        if (directory.empty())
            directory = ".";
        directory = fs::absolute(directory, error).lexically_normal();
        std::array<char, 1024> entered{};
        copy_path(entered,
                  !suggested_name.empty() ? directory / suggested_name
                                          : initial);
        if (directory_only)
            copy_path(entered, directory);

        const auto desktop = desktop_rect();
        const int width = std::min(640, int(desktop.d.w) - 24);
        const int row_height = std::max(24, int(runtime.char_h) + 8);
        const int rows =
            std::clamp((int(desktop.d.h) - 230) / row_height, 4, 12);
        const int footer = 105 + rows * row_height;
        dialog_form form(*dialog.get_owner(), width, footer + 100);
        form.label(caption(dialog.get_title(), 64),
                   16,
                   10,
                   width - 32,
                   row_height);
        const int folder_label =
            form.label("", 16, 38, width - 32, row_height);
        const int path_edit = form.entry(entered.data(),
                                         entered.size(),
                                         16,
                                         69,
                                         width - 32,
                                         row_height);
        form.box(16, 104, width - 32, rows * row_height);
        std::vector<int> choices;
        for (int row = 0; row < rows; ++row)
            choices.push_back(form.choice("",
                                          20,
                                          105 + row * row_height,
                                          width - 40,
                                          row_height - 2));
        const int up =
            form.button("Parent", 16, footer + 4, 88, row_height);
        const int previous =
            form.button("<", 116, footer + 4, 36, row_height);
        const int next =
            form.button(">", 160, footer + 4, 36, row_height);
        const int filter_button = form.button(
            "All files", 208, footer + 4, width - 224, row_height);
        const int status_label =
            form.label("", 16, footer + 34, width - 32, row_height);
        const int accept = form.button(directory_only ? "Select"
                                       : save         ? "Save"
                                                      : "Open",
                                       width - 210,
                                       footer + 64,
                                       88,
                                       row_height,
                                       true);
        const int cancel = form.button(
            "Cancel", width - 104, footer + 64, 88, row_height);
        int filter = 0;
        std::size_t page = 0;
        std::string status;
        std::vector<file_item> items;
        bool reload = true;
        for (;;) {
            if (reload) {
                items.clear();
                error.clear();
                fs::directory_iterator iterator(directory, error), end;
                for (; !error && iterator != end;
                     iterator.increment(error)) {
                    std::error_code type_error;
                    const bool is_directory =
                        iterator->is_directory(type_error);
                    if (type_error ||
                        (!is_directory &&
                         (directory_only ||
                          !matches(
                              dialog,
                              filter,
                              iterator->path().filename().string()))))
                        continue;
                    items.push_back({iterator->path(), is_directory});
                }
                std::sort(items.begin(),
                          items.end(),
                          [](const auto &left, const auto &right) {
                              if (left.directory != right.directory)
                                  return left.directory >
                                         right.directory;
                              return left.path.filename() <
                                     right.path.filename();
                          });
                if (error)
                    status = "Cannot read this folder.";
                page = std::min(
                    page,
                    items.empty() ? 0 : (items.size() - 1) / rows);
                reload = false;
            }
            form.set_label(
                folder_label,
                caption(directory.string(), (width - 32) / 8));
            for (int row = 0; row < rows; ++row) {
                const auto index = page * rows + row;
                form.set_enabled(choices[row], index < items.size());
                form.set_label(
                    choices[row],
                    index < items.size()
                        ? caption(
                              (items[index].directory ? "[Folder] "
                                                      : "") +
                                  items[index].path.filename().string(),
                              (width - 40) / 8)
                        : "");
            }
            form.set_enabled(previous, page > 0);
            form.set_enabled(next, (page + 1) * rows < items.size());
            form.set_enabled(filter_button,
                             !directory_only &&
                                 dialog.get_filters().size() > 1);
            form.set_label(
                filter_button,
                dialog.get_filters().empty()
                    ? "All files"
                    : caption(dialog.get_filters()[filter].name,
                              (width - 232) / 8));
            form.set_label(status_label,
                           caption(status, (width - 32) / 8));
            const int action = form.run(path_edit);
            if (action == cancel || action < 0)
                return {};
            status.clear();
            if (action == previous) {
                --page;
                continue;
            }
            if (action == next) {
                ++page;
                continue;
            }
            if (action == filter_button) {
                filter = (filter + 1) % dialog.get_filters().size();
                page = 0;
                reload = true;
                continue;
            }
            fs::path selected;
            const auto row =
                std::find(choices.begin(), choices.end(), action);
            if (action == up)
                selected = directory.parent_path();
            else if (row != choices.end()) {
                const auto index =
                    page * rows + std::size_t(row - choices.begin());
                if (index >= items.size())
                    continue;
                selected = items[index].path;
                if (!items[index].directory) {
                    copy_path(entered, selected);
                    continue;
                }
            } else if (action == accept) {
                selected = fs::path(entered.data());
                if (selected.empty()) {
                    status = "Enter or select a path.";
                    continue;
                }
                if (selected.is_relative())
                    selected = directory / selected;
                selected = selected.lexically_normal();
                if (directory_only) {
                    if (fs::is_directory(selected, error))
                        return {true, selected.string()};
                    status = "The selected folder does not exist.";
                    continue;
                }
                if (!fs::is_directory(selected, error)) {
                    if (!save &&
                        !fs::is_regular_file(selected, error)) {
                        status = "The selected file does not exist.";
                        continue;
                    }
                    if (save) {
                        selected = add_default_extension(
                            selected.string(), extension);
                        if (!fs::is_directory(selected.parent_path(),
                                              error)) {
                            status = "The destination folder does not "
                                     "exist.";
                            continue;
                        }
                        if (confirm_overwrite &&
                            fs::exists(selected, error) &&
                            native::message_box::show(
                                *dialog.get_owner(),
                                "The selected file already exists. "
                                "Replace it?",
                                "Replace file",
                                native::message_box_buttons::yes_no,
                                native::message_box_icon::question) !=
                                native::message_box_result::yes)
                            continue;
                    }
                    return {true, selected.string()};
                }
            } else
                continue;
            if (selected.empty())
                selected = directory;
            directory = selected;
            copy_path(entered, directory);
            page = 0;
            reload = true;
        }
    }

    std::string add_default_extension(const std::string &path,
                                      const std::string &extension) {
        if (path.empty() || extension.empty())
            return path;
        fs::path result(path);
        if (!result.has_extension())
            result +=
                extension.front() == '.' ? extension : "." + extension;
        return result.string();
    }
} // namespace linux::gemix

namespace native
{
    // The native form loop is synchronous and releases resources on
    // exit.
    void file_dialog::cancel_native_dialog() {}
} // namespace native
