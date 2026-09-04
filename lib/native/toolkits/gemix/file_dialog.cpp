//
// Implements GEM AES standard file-selector setup, result conversion,
// extension handling, and overwrite confirmation.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "file_dialog_common.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <string>

#include <gem.h>

#include <native/file_dialog.h>

namespace
{
    constexpr std::size_t path_capacity = 1024;
    constexpr std::size_t name_capacity = 256;

    // AES exposes one wildcard at a time. Prefer an unrestricted filter
    // when the portable dialog offers one, so callers can still reach
    // files
    // outside the first typed filter.
    std::string first_pattern(const native::file_dialog &dialog) {
        const auto &filters = dialog.get_filters();
        for (const auto &filter : filters) {
            for (const std::string &pattern : filter.patterns) {
                if (pattern == "*" || pattern == "*.*")
                    return "*";
            }
        }
        if (!filters.empty() && !filters.front().patterns.empty())
            return filters.front().patterns.front();
        return "*";
    }

    // Copy portable text into a fixed AES buffer with termination.
    void copy_text(char *target,
                   std::size_t capacity,
                   const std::string &text) {
        if (!target || capacity == 0)
            return;
        const std::size_t count =
            std::min(capacity - 1, text.size());
        std::memcpy(target, text.data(), count);
        target[count] = '\0';
    }

    // Convert the initial portable path into an AES wildcard path.
    std::string prepare_path(const native::file_dialog &dialog,
                             std::string &selection) {
        std::string path = dialog.get_initial_path().string();
        const std::string pattern = first_pattern(dialog);
        if (path.empty())
            return pattern;
        if (path.find('*') != std::string::npos ||
            path.find('?') != std::string::npos)
            return path;

        std::filesystem::path initial(path);
        if (initial.has_extension()) {
            selection = initial.filename().string();
            initial = initial.parent_path();
        }
        return (initial / pattern).string();
    }

    // Replace the selector wildcard with the selected leaf name.
    std::string combine_path(const std::string &mask,
                             const std::string &selection) {
        const std::filesystem::path parent =
            std::filesystem::path(mask).parent_path();
        if (parent.empty())
            return selection;
        return (parent / selection).string();
    }
} // namespace

namespace linux::gemix
{
    file_dialog_response show_file_dialog(
        const native::file_dialog &dialog,
        const std::string &suggested_name) {
        std::string selection = suggested_name;
        const std::string initial = prepare_path(dialog, selection);

        std::array<char, path_capacity> path = {};
        std::array<char, name_capacity> name = {};
        copy_text(path.data(), path.size(), initial);
        copy_text(name.data(), name.size(), selection);

        WORD button = 0;
        file_dialog_response response;
        if (fsel_input(path.data(), name.data(), &button) == 0 ||
            button == 0 || name[0] == '\0')
            return response;

        response.path = combine_path(path.data(), name.data());
        response.accepted = !response.path.empty();
        return response;
    }

    std::string add_default_extension(
        const std::string &path, const std::string &extension) {
        if (path.empty() || extension.empty())
            return path;
        std::filesystem::path result(path);
        if (result.has_extension())
            return path;
        result += extension.front() == '.' ? extension
                                           : "." + extension;
        return result.string();
    }

    bool confirm_file_overwrite(const std::string &path) {
        std::error_code error;
        if (!std::filesystem::is_regular_file(path, error))
            return true;

        char alert[] =
            "[2][The selected file already exists.|Replace it?]"
            "[Replace|Cancel]";
        return form_alert(2, alert) == 1;
    }
} // namespace linux::gemix

namespace native
{
    void file_dialog::cancel_native_dialog() {}
} // namespace native
