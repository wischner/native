//
// Implements SDL2 file-browser discovery, icon loading, history, selection,
// validation, filtering, and mode-specific path acceptance.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "file_browser.h"

#include <algorithm>
#include <cctype>
#include <functional>
#include <iomanip>
#include <sstream>
#include <system_error>
#include <utility>

#include <native/filesystem.h>

#include "../../file_filter_match.h"
#include "../../platforms/linux/file_dialog_process.h"

namespace
{
    namespace fs = std::filesystem;

    constexpr native::dim place_icon_size = 20;
    constexpr native::dim entry_icon_size = 20;

    // Normalize and, when possible, absolutize a path without requiring it
    // or its final component to exist.
    fs::path normalize_path(const fs::path &path) {
        if (path.empty())
            return {};
        std::error_code error;
        const fs::path absolute = fs::absolute(path, error);
        return (error ? path : absolute).lexically_normal();
    }

    // Return the process current directory or the filesystem root fallback.
    fs::path current_directory() {
        std::error_code error;
        const fs::path current = fs::current_path(error);
        if (!error && !current.empty())
            return normalize_path(current);
        return fs::path("/");
    }

    // Query one path and report whether it names a directory.
    bool path_is_directory(const fs::path &path) {
        std::error_code error;
        return fs::is_directory(path, error);
    }

    // Query one path and report whether it names a regular file.
    bool path_is_regular_file(const fs::path &path) {
        std::error_code error;
        return fs::is_regular_file(path, error);
    }

    // Query one path and report whether any filesystem entry exists.
    bool path_exists(const fs::path &path) {
        std::error_code error;
        return fs::exists(path, error);
    }

    // Return a case-folded ASCII value for display ordering and icon keys.
    std::string lowercase(std::string text) {
        std::transform(
            text.begin(), text.end(), text.begin(),
            [](unsigned char value) {
                return static_cast<char>(std::tolower(value));
            });
        return text;
    }

    // Decode an exact-size icon resource into shared icon-view storage.
    std::shared_ptr<const native::img> decode_icon(
        const native::file_icon &icon) {
        const auto &png = icon.get_png();
        return std::shared_ptr<const native::img>(
            new native::img(native::img::decode(png.data(), png.size())));
    }

    // Return whether a filename is accepted by at least one dialog filter.
    bool matches_filters(const native::file_dialog &dialog,
                         const std::string &name) {
        if (dialog.get_filters().empty())
            return true;
        for (const native::file_filter &filter : dialog.get_filters()) {
            for (const std::string &pattern : filter.patterns) {
                if (native::detail::matches_file_pattern(pattern, name))
                    return true;
            }
        }
        return false;
    }

    // Return whether a system location belongs in the concise Places pane.
    bool is_visible_place(native::special_directory_kind kind) {
        using kind_t = native::special_directory_kind;
        switch (kind) {
        case kind_t::home:
        case kind_t::desktop:
        case kind_t::documents:
        case kind_t::downloads:
        case kind_t::music:
        case kind_t::pictures:
        case kind_t::videos:
        case kind_t::public_share:
        case kind_t::templates:
        case kind_t::applications:
        case kind_t::temporary:
            return true;
        default:
            return false;
        }
    }

    // Return a concise type label suitable for a file-table column.
    std::string type_label(const fs::path &path, bool directory) {
        if (directory)
            return "Folder";
        std::string extension = path.extension().string();
        if (extension.empty())
            return "File";
        if (extension.front() == '.')
            extension.erase(extension.begin());
        std::transform(
            extension.begin(), extension.end(), extension.begin(),
            [](unsigned char value) {
                return static_cast<char>(std::toupper(value));
            });
        return extension + " file";
    }

    // Format a file byte count as a compact desktop-style size label.
    std::string size_label(const fs::path &path, bool directory) {
        if (directory)
            return {};
        std::error_code error;
        const std::uintmax_t bytes = fs::file_size(path, error);
        if (error)
            return {};
        constexpr std::uintmax_t kibibyte = 1024;
        constexpr std::uintmax_t mebibyte = kibibyte * 1024;
        constexpr std::uintmax_t gibibyte = mebibyte * 1024;
        if (bytes < kibibyte)
            return std::to_string(bytes) + " B";
        const std::uintmax_t divisor = bytes >= gibibyte
            ? gibibyte
            : bytes >= mebibyte ? mebibyte : kibibyte;
        const char *suffix = bytes >= gibibyte
            ? " GB"
            : bytes >= mebibyte ? " MB" : " KB";
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(
            bytes < divisor * 10 ? 1 : 0)
               << static_cast<double>(bytes) /
                      static_cast<double>(divisor)
               << suffix;
        return stream.str();
    }
} // namespace

namespace linux::sdl2
{
    void file_browser::build_places() {
        _place_entries.clear();
        std::vector<native::table_store_row> rows;
        const auto &directories = native::special_directory::detect();
        for (const native::special_directory &directory : directories) {
            if (!is_visible_place(directory.get_kind()) ||
                !path_is_directory(directory.get_path())) {
                continue;
            }
            const fs::path path = normalize_path(directory.get_path());
            const auto duplicate = std::find_if(
                _place_entries.begin(), _place_entries.end(),
                [&path](const file_browser_place &entry) {
                    return entry.path == path;
                });
            if (duplicate != _place_entries.end())
                continue;
            auto image = decode_icon(directory.get_icon(place_icon_size));
            _place_entries.push_back(
                {directory.get_name(), path, image});
            rows.push_back({
                static_cast<native::table_row_id>(rows.size() + 1),
                {{1, {directory.get_name(), image.get()}}}});
        }

        fs::path root = current_directory().root_path();
        if (root.empty())
            root = fs::path("/");
        root = normalize_path(root);
        const auto duplicate = std::find_if(
            _place_entries.begin(), _place_entries.end(),
            [&root](const file_browser_place &entry) {
                return entry.path == root;
            });
        if (duplicate == _place_entries.end()) {
            auto image = decode_icon(
                native::file_icon::for_directory(root, place_icon_size));
            _place_entries.push_back({"Computer", root, image});
            rows.push_back({
                static_cast<native::table_row_id>(rows.size() + 1),
                {{1, {"Computer", image.get()}}}});
        }
        _place_store.set_rows(std::move(rows));
    }

    void file_browser::choose_initial_directory() {
        fs::path initial = _source.get_initial_path();
        std::string initial_name;
        if (initial.empty())
            initial = current_directory();
        initial = normalize_path(initial);
        if (!path_is_directory(initial)) {
            if (!_directory_mode && _name.get_text().empty())
                initial_name = initial.filename().string();
            initial = initial.parent_path();
        }
        if (!path_is_directory(initial))
            initial = current_directory();
        _history = {initial};
        _history_index = 0;
        show_directory(initial);
        if (!initial_name.empty())
            _name.set_text(initial_name);
    }

    bool file_browser::valid_place(native::table_row_id id) const {
        return id != native::invalid_table_row_id &&
               id <= _place_entries.size();
    }

    bool file_browser::valid_entry(native::table_row_id id) const {
        return id != native::invalid_table_row_id &&
               id <= _browser_entries.size();
    }

    std::shared_ptr<const native::img> file_browser::icon_for(
        const file_browser_entry &entry) {
        std::string key;
        if (entry.directory)
            key = "folder";
        else
            key = "file:" + lowercase(entry.path.extension().string());
        const auto found = _icon_cache.find(key);
        if (found != _icon_cache.end())
            return found->second;
        const native::file_icon icon = entry.directory
            ? native::file_icon::for_directory(
                  entry.path, entry_icon_size)
            : native::file_icon::for_file(entry.path, entry_icon_size);
        auto image = decode_icon(icon);
        _icon_cache.emplace(std::move(key), image);
        return image;
    }

    void file_browser::populate() {
        _browser_entries.clear();
        _status.clear();
        std::error_code error;
        fs::directory_iterator iterator(
            _current,
            fs::directory_options::skip_permission_denied,
            error);
        const fs::directory_iterator end;
        while (!error && iterator != end) {
            const fs::directory_entry &candidate = *iterator;
            std::error_code type_error;
            const bool directory = candidate.is_directory(type_error);
            const std::string leaf =
                candidate.path().filename().string();
            const bool hidden = !leaf.empty() && leaf.front() == '.';
            if (!type_error && (_show_hidden || !hidden) &&
                (directory ||
                 (!_directory_mode && matches_filters(_source, leaf)))) {
                _browser_entries.push_back(
                    {leaf, candidate.path().lexically_normal(),
                     directory, {},
                     type_label(candidate.path(), directory),
                     size_label(candidate.path(), directory)});
            }
            iterator.increment(error);
        }
        if (error)
            _status = "This location cannot be opened.";
        std::sort(
            _browser_entries.begin(), _browser_entries.end(),
            [](const file_browser_entry &left,
               const file_browser_entry &right) {
                if (left.directory != right.directory)
                    return left.directory > right.directory;
                const std::string left_name = lowercase(left.label);
                const std::string right_name = lowercase(right.label);
                return left_name == right_name
                    ? left.label < right.label
                    : left_name < right_name;
            });

        std::vector<native::table_store_row> rows;
        rows.reserve(_browser_entries.size());
        for (std::size_t index = 0;
             index < _browser_entries.size(); ++index) {
            file_browser_entry &entry = _browser_entries[index];
            entry.image = icon_for(entry);
            rows.push_back({
                static_cast<native::table_row_id>(index + 1),
                {{1, {entry.label, entry.image.get()}},
                 {2, {entry.type, nullptr}},
                 {3, {entry.size, nullptr}}}});
        }
        _entry_store.set_rows(std::move(rows));
        _entries.set_selected_rows({});
        _entries.on_native_scroll(0, 0);
        if (_status.empty()) {
            _status = std::to_string(_browser_entries.size()) +
                (_browser_entries.size() == 1 ? " item" : " items");
            if (_show_hidden)
                _status += " (hidden files shown)";
        }
        update_place_selection();
        _window.invalidate();
    }

    void file_browser::show_directory(const fs::path &path) {
        _current = normalize_path(path);
        _location.set_text(_current.string());
        _breadcrumbs.set_path(_current);
        if (!_save && !_directory_mode)
            _name.set_text({});
        _pending_overwrite.clear();
        populate();
    }

    void file_browser::navigate_to(const fs::path &path,
                                   bool remember) {
        const fs::path normalized = normalize_path(path);
        if (!path_is_directory(normalized)) {
            _status = "That folder is not available.";
            _window.invalidate();
            return;
        }
        if (remember &&
            (_history.empty() || _history[_history_index] != normalized)) {
            _history.erase(
                _history.begin() +
                    static_cast<std::ptrdiff_t>(_history_index + 1),
                _history.end());
            _history.push_back(normalized);
            _history_index = _history.size() - 1;
        }
        show_directory(normalized);
    }

    bool file_browser::move_history(int direction) {
        if (direction < 0 && _history_index > 0) {
            --_history_index;
            show_directory(_history[_history_index]);
        } else if (direction > 0 &&
                   _history_index + 1 < _history.size()) {
            ++_history_index;
            show_directory(_history[_history_index]);
        } else {
            _status = direction < 0
                ? "No earlier location."
                : "No later location.";
            _window.invalidate();
        }
        return true;
    }

    void file_browser::update_place_selection() {
        int selected = -1;
        for (std::size_t index = 0;
             index < _place_entries.size(); ++index) {
            if (_place_entries[index].path == _current) {
                selected = static_cast<int>(index);
                break;
            }
        }
        _places.set_selected_rows(
            selected < 0
                ? std::vector<native::table_row_id>{}
                : std::vector<native::table_row_id>{
                      static_cast<native::table_row_id>(selected + 1)});
    }

    void file_browser::selection_changed(native::table_row_id id) {
        _pending_overwrite.clear();
        if (!valid_entry(id))
            return;
        const file_browser_entry &entry = _browser_entries[id - 1];
        if (!_directory_mode && !entry.directory)
            _name.set_text(entry.label);
        else if (!_directory_mode && !_save)
            _name.set_text({});
        _status = entry.path.string();
        _window.invalidate();
    }

    bool file_browser::activate_entry(native::table_row_id id) {
        if (!valid_entry(id))
            return false;
        const file_browser_entry &entry = _browser_entries[id - 1];
        if (entry.directory)
            navigate_to(entry.path, true);
        else if (!_directory_mode) {
            _name.set_text(entry.label);
            accept_path(entry.path);
        }
        return true;
    }

    fs::path file_browser::entered_location() const {
        const fs::path entered(_location.get_text());
        if (entered.empty())
            return _current;
        return normalize_path(
            entered.is_absolute() ? entered : _current / entered);
    }

    fs::path file_browser::entered_file() const {
        const fs::path entered(_name.get_text());
        if (entered.empty())
            return {};
        return normalize_path(
            entered.is_absolute() ? entered : _current / entered);
    }

    bool file_browser::activate_location() {
        const fs::path path = entered_location();
        if (path_is_directory(path)) {
            navigate_to(path, true);
            hide_location_editor();
            return true;
        }
        if (!_directory_mode && path_is_regular_file(path)) {
            _current = path.parent_path();
            _name.set_text(path.filename().string());
            return accept_path(path);
        }
        _status = "Folder not found.";
        _window.invalidate();
        return true;
    }

    fs::path file_browser::selected_path() const {
        const auto rows = _entries.get_selected_rows();
        return !rows.empty() && valid_entry(rows.back())
            ? _browser_entries[rows.back() - 1].path
            : fs::path();
    }

    bool file_browser::accept() {
        if (_directory_mode) {
            const fs::path selected = selected_path();
            return accept_path(
                !selected.empty() && path_is_directory(selected)
                    ? selected
                    : _current);
        }

        fs::path path = entered_file();
        if (path.empty()) {
            const fs::path selected = selected_path();
            if (path_is_directory(selected)) {
                navigate_to(selected, true);
                return true;
            }
            path = selected;
        }
        if (path_is_directory(path)) {
            navigate_to(path, true);
            return true;
        }
        if (path.empty()) {
            _status = "No file selected.";
            _window.invalidate();
            return true;
        }
        return accept_path(path);
    }

    bool file_browser::accept_path(fs::path path) {
        if (_directory_mode) {
            if (!path_is_directory(path)) {
                _status = "The selected folder does not exist.";
                _window.invalidate();
                return true;
            }
        } else if (!_save) {
            if (!path_is_regular_file(path)) {
                _status = "The selected file does not exist.";
                _window.invalidate();
                return true;
            }
        } else {
            path = linux::add_default_extension(
                path, _default_extension);
            _name.set_text(path.filename().string());
            if (!path_is_directory(path.parent_path())) {
                _status = "The destination folder does not exist.";
                _window.invalidate();
                return true;
            }
            if (_confirm_overwrite && path_exists(path) &&
                _pending_overwrite != path) {
                _pending_overwrite = path;
                _status =
                    "File exists; choose Save again to replace it.";
                _window.invalidate();
                return true;
            }
        }

        _paths = {normalize_path(path)};
        if (_window.get_created())
            _window.close(native::dialog_result::accepted);
        return true;
    }
} // namespace linux::sdl2
