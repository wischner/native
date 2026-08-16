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
#include <fstream>
#include <string>

#include <gem.h>

#include <native/file_dialog.h>

namespace
{
    constexpr std::size_t path_capacity = 1024;
    constexpr std::size_t name_capacity = 256;

    // Select the first wildcard accepted by the AES file selector.
    std::string first_pattern(const native::file_dialog &dialog) {
        const auto &filters = dialog.get_filters();
        if (!filters.empty() && !filters.front().patterns.empty())
            return filters.front().patterns.front();
        return "*.*";
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
        std::string path = dialog.get_initial_path();
        std::replace(path.begin(), path.end(), '/', '\\');
        const std::string pattern = first_pattern(dialog);
        if (path.empty())
            return pattern;
        if (path.find('*') != std::string::npos ||
            path.find('?') != std::string::npos)
            return path;

        const std::size_t separator = path.find_last_of('\\');
        const std::size_t dot = path.find_last_of('.');
        if (dot != std::string::npos &&
            (separator == std::string::npos || dot > separator)) {
            selection = path.substr(separator == std::string::npos
                                        ? 0
                                        : separator + 1);
            path.erase(separator == std::string::npos ? 0
                                                      : separator + 1);
        } else if (path.back() != '\\') {
            path.push_back('\\');
        }
        path += pattern;
        return path;
    }

    // Replace the selector wildcard with the selected leaf name.
    std::string combine_path(const std::string &mask,
                             const std::string &selection) {
        const std::size_t separator = mask.find_last_of("/\\");
        if (separator == std::string::npos)
            return selection;
        return mask.substr(0, separator + 1) + selection;
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

    bool confirm_file_overwrite(const std::string &path) {
        std::ifstream input(path, std::ios::binary);
        if (!input.good())
            return true;

        char alert[] =
            "[2][The selected file already exists.|Replace it?]"
            "[Replace|Cancel]";
        return form_alert(2, alert) == 1;
    }
} // namespace linux::gemix

namespace native
{
    void file_dialog::cancel_native_dialog() const {}
} // namespace native
