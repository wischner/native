//
// Implements the normalized process snapshot of platform special folders.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/filesystem.h>

#include <algorithm>
#include <filesystem>
#include <utility>

#include "filesystem_backend.h"

namespace
{
    using kind = native::special_directory_kind;

    std::string name_for(kind value) {
        switch (value) {
        case kind::home:
            return "Home";
        case kind::desktop:
            return "Desktop";
        case kind::documents:
            return "Documents";
        case kind::downloads:
            return "Downloads";
        case kind::music:
            return "Music";
        case kind::pictures:
            return "Pictures";
        case kind::videos:
            return "Videos";
        case kind::public_share:
            return "Public";
        case kind::templates:
            return "Templates";
        case kind::applications:
            return "Applications";
        case kind::fonts:
            return "Fonts";
        case kind::configuration:
            return "Configuration";
        case kind::application_data:
            return "Application Data";
        case kind::cache:
            return "Cache";
        case kind::temporary:
            return "Temporary";
        }
        return {};
    }
} // namespace

namespace native
{
    std::vector<special_directory> special_directory::_directories;

    special_directory::special_directory(
        special_directory_kind kind,
        std::string name,
        std::filesystem::path path)
        : _kind(kind)
        , _name(std::move(name))
        , _path(std::move(path)) {}

    special_directory_kind special_directory::get_kind() const {
        return _kind;
    }

    const std::string &special_directory::get_name() const {
        return _name;
    }

    const std::filesystem::path &special_directory::get_path() const {
        return _path;
    }

    file_icon special_directory::get_icon(dim size) const {
        return file_icon::for_directory(_path, size);
    }

    const std::vector<special_directory> &special_directory::detect() {
        _directories.clear();
        std::vector<detail::special_directory_path> paths =
            detail::detect_platform_special_directories();

        std::error_code error;
        const std::filesystem::path temporary =
            std::filesystem::temp_directory_path(error);
        if (!error && !temporary.empty()) {
            paths.push_back(
                {special_directory_kind::temporary, temporary});
        }

        for (detail::special_directory_path &entry : paths) {
            if (entry.path.empty())
                continue;
            entry.path = entry.path.lexically_normal();
            const auto duplicate = std::find_if(
                _directories.begin(),
                _directories.end(),
                [&entry](const special_directory &candidate) {
                    return candidate.get_kind() == entry.kind;
                });
            if (duplicate == _directories.end()) {
                _directories.emplace_back(
                    special_directory(entry.kind,
                                      name_for(entry.kind),
                                      std::move(entry.path)));
            }
        }
        std::sort(
            _directories.begin(),
            _directories.end(),
            [](const special_directory &left,
               const special_directory &right) {
                return left.get_kind() < right.get_kind();
            });
        return _directories;
    }

    int special_directory::count() {
        return static_cast<int>(_directories.size());
    }

    special_directory *special_directory::at(int index) {
        if (index < 0 || index >= static_cast<int>(_directories.size()))
            return nullptr;
        return &_directories[static_cast<std::size_t>(index)];
    }

    special_directory *special_directory::find(
        special_directory_kind kind) {
        const auto found = std::find_if(
            _directories.begin(),
            _directories.end(),
            [kind](const special_directory &candidate) {
                return candidate.get_kind() == kind;
            });
        return found == _directories.end() ? nullptr : &*found;
    }
} // namespace native
