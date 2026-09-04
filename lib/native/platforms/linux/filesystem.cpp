//
// Implements Freedesktop file icons and special directories using the C++
// standard filesystem and stream libraries.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "../../filesystem_backend.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <mutex>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace
{
    namespace fs = std::filesystem;
    using directory_kind = native::special_directory_kind;

    std::string trim(std::string value) {
        const auto whitespace = [](unsigned char character) {
            return std::isspace(character) != 0;
        };
        value.erase(value.begin(),
                    std::find_if_not(value.begin(),
                                     value.end(),
                                     whitespace));
        value.erase(std::find_if_not(value.rbegin(),
                                     value.rend(),
                                     whitespace)
                        .base(),
                    value.end());
        return value;
    }

    fs::path environment_path(const char *name) {
        const char *value = std::getenv(name);
        return value && *value ? fs::path(value) : fs::path();
    }

    fs::path home_path() {
        const fs::path home = environment_path("HOME");
        return home.is_absolute() ? home : fs::path();
    }

    fs::path absolute_environment_path(const char *name,
                                       const fs::path &fallback) {
        fs::path value = environment_path(name);
        if (value.is_absolute())
            return value;
        return fallback.is_absolute() ? fallback : fs::path();
    }

    std::string setting_value(const fs::path &path,
                              const std::string &key) {
        std::ifstream input(path);
        std::string line;
        while (std::getline(input, line)) {
            const std::size_t equals = line.find('=');
            if (equals == std::string::npos ||
                trim(line.substr(0, equals)) != key) {
                continue;
            }
            std::string value = trim(line.substr(equals + 1));
            if (value.size() >= 2 &&
                ((value.front() == '"' && value.back() == '"') ||
                 (value.front() == '\'' && value.back() == '\''))) {
                value = value.substr(1, value.size() - 2);
            }
            return value;
        }
        return {};
    }

    std::vector<std::string> split(std::string value, char separator) {
        std::vector<std::string> result;
        std::size_t begin = 0;
        for (;;) {
            const std::size_t end = value.find(separator, begin);
            std::string part = trim(value.substr(
                begin,
                end == std::string::npos ? std::string::npos
                                         : end - begin));
            if (!part.empty())
                result.push_back(std::move(part));
            if (end == std::string::npos)
                return result;
            begin = end + 1;
        }
    }

    void append_unique(std::vector<std::string> &values,
                       std::string value) {
        if (!value.empty() &&
            std::find(values.begin(), values.end(), value) == values.end()) {
            values.push_back(std::move(value));
        }
    }

    void append_unique(std::vector<fs::path> &values, fs::path value) {
        if (!value.empty() &&
            std::find(values.begin(), values.end(), value) == values.end()) {
            values.push_back(std::move(value));
        }
    }

    std::vector<fs::path> icon_roots() {
        std::vector<fs::path> roots;
        const fs::path home = home_path();
        if (!home.empty())
            append_unique(roots, home / ".icons");

        const fs::path data_home = absolute_environment_path(
            "XDG_DATA_HOME", home / ".local/share");
        if (!data_home.empty())
            append_unique(roots, data_home / "icons");

        const char *data_dirs = std::getenv("XDG_DATA_DIRS");
        const std::string directories =
            data_dirs && *data_dirs ? data_dirs : "/usr/local/share:/usr/share";
        for (const std::string &directory : split(directories, ':')) {
            const fs::path path(directory);
            if (path.is_absolute())
                append_unique(roots, path / "icons");
        }
        append_unique(roots, fs::path("/usr/share/pixmaps"));
        return roots;
    }

    std::vector<std::string> icon_themes(
        const std::vector<fs::path> &roots) {
        std::vector<std::string> themes;
        const fs::path home = home_path();
        const fs::path config_home = absolute_environment_path(
            "XDG_CONFIG_HOME", home / ".config");
        if (!config_home.empty()) {
            for (const char *relative : {"gtk-4.0/settings.ini",
                                         "gtk-3.0/settings.ini"}) {
                append_unique(themes,
                              setting_value(config_home / relative,
                                            "gtk-icon-theme-name"));
            }
        }
        append_unique(themes, "Adwaita");

        for (std::size_t index = 0; index < themes.size(); ++index) {
            for (const fs::path &root : roots) {
                const std::string inherited = setting_value(
                    root / themes[index] / "index.theme", "Inherits");
                for (std::string theme : split(inherited, ','))
                    append_unique(themes, std::move(theme));
            }
        }
        append_unique(themes, "hicolor");
        return themes;
    }

    int nominal_icon_size(const fs::path &path) {
        for (const fs::path &component : path) {
            const std::string text = component.string();
            if (text.empty() || !std::isdigit(
                                    static_cast<unsigned char>(text[0]))) {
                continue;
            }
            std::size_t used = 0;
            try {
                const int value = std::stoi(text, &used);
                if (used > 0 && value > 0)
                    return value;
            } catch (...) {
            }
        }
        return 0;
    }

    fs::path find_icon_uncached(const std::string &name,
                                native::dim requested_size) {
        if (name.empty())
            return {};
        const fs::path supplied(name);
        std::error_code error;
        if (supplied.is_absolute() &&
            fs::is_regular_file(supplied, error) &&
            supplied.extension() == ".png") {
            return supplied;
        }

        const std::vector<fs::path> roots = icon_roots();
        const std::vector<std::string> themes = icon_themes(roots);
        fs::path best;
        int best_score = std::numeric_limits<int>::max();
        for (const std::string &theme : themes) {
            for (const fs::path &root : roots) {
                const fs::path theme_root = root / theme;
                error.clear();
                if (!fs::is_directory(theme_root, error))
                    continue;
                fs::recursive_directory_iterator iterator(
                    theme_root,
                    fs::directory_options::skip_permission_denied,
                    error);
                const fs::recursive_directory_iterator end;
                while (!error && iterator != end) {
                    const fs::directory_entry &entry = *iterator;
                    if (entry.path().filename() == name + ".png") {
                        const int nominal = nominal_icon_size(
                            entry.path().lexically_relative(theme_root));
                        const int difference = nominal == 0
                                                   ? 10000
                                                   : std::abs(
                                                         nominal -
                                                         requested_size);
                        const int undersize =
                            nominal != 0 && nominal < requested_size
                                ? 1000
                                : 0;
                        const int score = difference + undersize;
                        if (score < best_score) {
                            best = entry.path();
                            best_score = score;
                        }
                    }
                    iterator.increment(error);
                }
                if (!best.empty() && best_score == 0)
                    return best;
            }
        }

        for (const fs::path &root : roots) {
            error.clear();
            const fs::path candidate = root / (name + ".png");
            if (fs::is_regular_file(candidate, error))
                return candidate;
        }
        return best;
    }

    fs::path find_icon(const std::string &name,
                       native::dim requested_size) {
        static std::mutex cache_mutex;
        static std::map<std::pair<std::string, native::dim>, fs::path>
            cache;
        const auto key = std::make_pair(name, requested_size);
        {
            const std::lock_guard<std::mutex> lock(cache_mutex);
            const auto found = cache.find(key);
            if (found != cache.end())
                return found->second;
        }
        fs::path result = find_icon_uncached(name, requested_size);
        const std::lock_guard<std::mutex> lock(cache_mutex);
        cache.emplace(key, result);
        return result;
    }

    std::string lower_extension(const fs::path &path) {
        std::string extension = path.extension().string();
        std::transform(extension.begin(),
                       extension.end(),
                       extension.begin(),
                       [](unsigned char value) {
                           return static_cast<char>(std::tolower(value));
                       });
        return extension;
    }

    bool has_executable_permission(const fs::path &path) {
        std::error_code error;
        const fs::perms permissions = fs::status(path, error).permissions();
        if (error)
            return false;
        constexpr fs::perms executable = fs::perms::owner_exec |
                                         fs::perms::group_exec |
                                         fs::perms::others_exec;
        return (permissions & executable) != fs::perms::none;
    }

    std::string icon_name_for(const fs::path &path, bool directory) {
        const fs::path home = home_path();
        if (directory) {
            if (!home.empty() &&
                path.lexically_normal() == home.lexically_normal()) {
                return "user-home";
            }
            const std::string custom = setting_value(
                path / ".directory", "Icon");
            return custom.empty() ? "folder" : custom;
        }

        const std::string extension = lower_extension(path);
        if (extension == ".desktop") {
            const std::string custom = setting_value(path, "Icon");
            if (!custom.empty())
                return custom;
        }
        if (extension == ".png" || extension == ".jpg" ||
            extension == ".jpeg" || extension == ".gif" ||
            extension == ".bmp" || extension == ".webp" ||
            extension == ".svg") {
            return "image-x-generic";
        }
        if (extension == ".mp3" || extension == ".wav" ||
            extension == ".flac" || extension == ".ogg" ||
            extension == ".m4a") {
            return "audio-x-generic";
        }
        if (extension == ".mp4" || extension == ".mkv" ||
            extension == ".avi" || extension == ".mov" ||
            extension == ".webm") {
            return "video-x-generic";
        }
        if (extension == ".pdf")
            return "application-pdf";
        if (extension == ".zip" || extension == ".gz" ||
            extension == ".bz2" || extension == ".xz" ||
            extension == ".tar" || extension == ".7z" ||
            extension == ".rar") {
            return "package-x-generic";
        }
        if (extension == ".cpp" || extension == ".cc" ||
            extension == ".c" || extension == ".h" ||
            extension == ".hpp" || extension == ".rs" ||
            extension == ".py" || extension == ".js") {
            return "text-x-source";
        }
        if (extension == ".txt" || extension == ".md" ||
            extension == ".log" || extension == ".csv" ||
            extension == ".json" || extension == ".xml") {
            return "text-x-generic";
        }
        if (has_executable_permission(path))
            return "application-x-executable";
        return "unknown";
    }

    bool copy_icon(const fs::path &path, native::img &target) {
        try {
            const native::img source = native::img::load(path);
            target.get_gpx()
                .clear(native::rgba(0, 0, 0, 0))
                .draw_img(source,
                          native::rect(0,
                                       0,
                                       target.w(),
                                       target.h()),
                          native::image_filter::linear);
            return true;
        } catch (...) {
            return false;
        }
    }

    fs::path expand_user_directory(std::string value,
                                   const fs::path &home) {
        value = trim(std::move(value));
        if (value.size() >= 2 && value.front() == '"' &&
            value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }
        constexpr const char *home_variable = "$HOME";
        if (value.starts_with(home_variable)) {
            value.erase(0, std::char_traits<char>::length(home_variable));
            if (!value.empty() && value.front() == '/')
                value.erase(value.begin());
            return home / value;
        }
        const fs::path result(value);
        return result.is_absolute() ? result : fs::path();
    }

    std::vector<std::pair<std::string, fs::path>> user_directories(
        const fs::path &home,
        const fs::path &config_home) {
        std::vector<std::pair<std::string, fs::path>> result;
        if (config_home.empty())
            return result;
        std::ifstream input(config_home / "user-dirs.dirs");
        std::string line;
        while (std::getline(input, line)) {
            const std::size_t equals = line.find('=');
            if (equals == std::string::npos)
                continue;
            const std::string key = trim(line.substr(0, equals));
            if (!key.starts_with("XDG_") ||
                !key.ends_with("_DIR")) {
                continue;
            }
            fs::path path = expand_user_directory(
                line.substr(equals + 1), home);
            result.emplace_back(key, std::move(path));
        }
        return result;
    }

    fs::path configured_user_path(
        const std::vector<std::pair<std::string, fs::path>> &configured,
        const std::string &key,
        const fs::path &fallback) {
        const auto found = std::find_if(
            configured.begin(), configured.end(),
            [&key](const auto &entry) { return entry.first == key; });
        return found == configured.end() ? fallback : found->second;
    }
} // namespace

namespace native::detail
{
    bool load_native_file_icon(const std::filesystem::path &path,
                               bool directory,
                               img &target) {
        const std::string name = icon_name_for(path, directory);
        fs::path icon = find_icon(name, static_cast<dim>(target.w()));
        if (icon.empty() && name != (directory ? "folder" : "text-x-generic")) {
            icon = find_icon(directory ? "folder" : "text-x-generic",
                             static_cast<dim>(target.w()));
        }
        return !icon.empty() && copy_icon(icon, target);
    }

    std::vector<special_directory_path>
    detect_platform_special_directories() {
        std::vector<special_directory_path> result;
        const fs::path home = home_path();
        const fs::path config_home = absolute_environment_path(
            "XDG_CONFIG_HOME", home / ".config");
        const fs::path data_home = absolute_environment_path(
            "XDG_DATA_HOME", home / ".local/share");
        const fs::path cache_home = absolute_environment_path(
            "XDG_CACHE_HOME", home / ".cache");
        const auto configured = user_directories(home, config_home);

        if (!home.empty()) {
            result.push_back({directory_kind::home, home});
            result.push_back({directory_kind::desktop,
                              configured_user_path(configured,
                                                   "XDG_DESKTOP_DIR",
                                                   home / "Desktop")});
            result.push_back({directory_kind::documents,
                              configured_user_path(configured,
                                                   "XDG_DOCUMENTS_DIR",
                                                   home / "Documents")});
            result.push_back({directory_kind::downloads,
                              configured_user_path(configured,
                                                   "XDG_DOWNLOAD_DIR",
                                                   home / "Downloads")});
            result.push_back({directory_kind::music,
                              configured_user_path(configured,
                                                   "XDG_MUSIC_DIR",
                                                   home / "Music")});
            result.push_back({directory_kind::pictures,
                              configured_user_path(configured,
                                                   "XDG_PICTURES_DIR",
                                                   home / "Pictures")});
            result.push_back({directory_kind::videos,
                              configured_user_path(configured,
                                                   "XDG_VIDEOS_DIR",
                                                   home / "Videos")});
            result.push_back({directory_kind::public_share,
                              configured_user_path(configured,
                                                   "XDG_PUBLICSHARE_DIR",
                                                   home / "Public")});
            result.push_back({directory_kind::templates,
                              configured_user_path(configured,
                                                   "XDG_TEMPLATES_DIR",
                                                   home / "Templates")});
        }
        if (!data_home.empty()) {
            result.push_back(
                {directory_kind::applications,
                 data_home / "applications"});
            result.push_back(
                {directory_kind::fonts, data_home / "fonts"});
            result.push_back(
                {directory_kind::application_data, data_home});
        }
        if (!config_home.empty()) {
            result.push_back(
                {directory_kind::configuration, config_home});
        }
        if (!cache_home.empty())
            result.push_back({directory_kind::cache, cache_home});
        return result;
    }
} // namespace native::detail
