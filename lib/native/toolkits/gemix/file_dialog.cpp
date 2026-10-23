//
// Adapts the pinned GEM SDK's extended file, save and folder selectors.
// AES owns their layout, filtering, editing and synchronous modal loop.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "file_dialog_common.h"
#include "globals.h"
#include <algorithm>
#include <array>
#include <filesystem>
#include <native/message_box.h>

namespace linux::gemix
{
    file_dialog_response
    show_file_dialog(native::file_dialog &dialog,
                     const std::string &suggested_name,
                     bool save,
                     bool directory_only,
                     const std::string &extension,
                     bool confirm_overwrite) {
        namespace fs = std::filesystem;
        std::error_code error;
        fs::path directory = dialog.get_initial_path();
        std::string selection = suggested_name;
        if (directory.empty())
            directory = fs::current_path();
        if (!directory_only && !fs::is_directory(directory, error)) {
            if (selection.empty())
                selection = directory.filename().string();
            directory = directory.parent_path();
        }
        if (directory.empty())
            directory = fs::current_path();
        std::string pattern = "*";
        if (!dialog.get_filters().empty() &&
            !dialog.get_filters().front().patterns.empty())
            pattern = dialog.get_filters().front().patterns.front();
        std::array<char, 4096> path{}, name{};
        const auto copy = [](auto &buffer, const std::string &text) {
            std::copy_n(text.begin(),
                        std::min(text.size(), buffer.size() - 1),
                        buffer.begin());
        };
        copy(path,
             directory_only ? directory.string()
                            : (directory / pattern).string());
        copy(name, selection);
        WORD button = 0;
        flush_repaints();
        const WORD ran =
            directory_only ? fsel_directory(path.data(), &button)
            : save ? fsel_save(path.data(), name.data(), &button)
                   : fsel_exinput(path.data(),
                                  name.data(),
                                  &button,
                                  const_cast<char *>(
                                      dialog.get_title().c_str()));
        for (auto *window : windows)
            request_repaint(window);
        if (!ran || !button || (!directory_only && !name[0]))
            return {};
        fs::path selected = directory_only
                                ? fs::path(path.data())
                                : fs::path(path.data()).parent_path() /
                                      fs::path(name.data());
        if (save) {
            const auto original = selected;
            selected =
                add_default_extension(selected.string(), extension);
            // fsel_save confirms its returned filename. Confirm only
            // when extension completion changes that destination
            // afterwards.
            if (confirm_overwrite && selected != original &&
                fs::exists(selected, error) &&
                native::message_box::show(
                    *dialog.get_owner(),
                    "The selected file already exists. Replace it?",
                    "Replace file",
                    native::message_box_buttons::yes_no,
                    native::message_box_icon::question) !=
                    native::message_box_result::yes)
                return {};
        }
        return {true, selected.lexically_normal().string()};
    }

    std::string add_default_extension(const std::string &path,
                                      const std::string &extension) {
        if (path.empty() || extension.empty())
            return path;
        std::filesystem::path result(path);
        if (!result.has_extension())
            result +=
                extension.front() == '.' ? extension : "." + extension;
        return result.string();
    }
} // namespace linux::gemix
namespace native
{
    // AES selectors finish synchronously before returning to Native.
    void file_dialog::cancel_native_dialog() {}
} // namespace native
