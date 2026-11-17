//
// Reads settings with standard streams and publishes same-filesystem
// replacements with std::filesystem. POSIX descriptors are used only for
// advisory writer locking; a process mutex in configuration.cpp serializes
// threads, and the kernel releases the advisory lock if a process exits.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "configuration_file.h"

#include <array>
#include <cerrno>
#include <fstream>
#include <random>
#include <stdexcept>
#include <system_error>
#include <utility>
#include <fcntl.h>
#include <unistd.h>

namespace
{
    namespace fs = std::filesystem;

    class writer_lock
    {
    public:
        // The stable lock file is never renamed or removed by the library.
        explicit writer_lock(const fs::path &path) {
            std::ofstream file(path, std::ios::binary | std::ios::app);
            file.close();
            if (!file)
                throw std::runtime_error("Cannot create configuration lock: " + path.string());
            _fd = ::open(path.c_str(), O_RDWR | O_CLOEXEC);
            if (_fd < 0)
                throw std::system_error(errno, std::generic_category(), "Open configuration lock");
            struct flock request{};
            request.l_type = F_WRLCK;
            request.l_whence = SEEK_SET;
            while (::fcntl(_fd, F_SETLKW, &request) < 0) {
                if (errno == EINTR)
                    continue;
                const int error = errno;
                ::close(_fd);
                _fd = -1;
                throw std::system_error(error, std::generic_category(), "Lock configuration");
            }
        }
        ~writer_lock() { if (_fd >= 0) ::close(_fd); }
        writer_lock(const writer_lock &) = delete;
        writer_lock &operator=(const writer_lock &) = delete;
    private:
        int _fd = -1;
    };

    // Exclusive staging directories avoid clobbering another writer's data.
    class staging_file
    {
    public:
        explicit staging_file(const fs::path &parent) {
            std::random_device random;
            for (int attempt = 0; attempt < 32; ++attempt) {
                const auto candidate = parent /
                    (".native-settings-" + std::to_string(random()) +
                     "-" + std::to_string(random()));
                if (fs::create_directory(candidate)) {
                    _directory = candidate;
                    try {
                        fs::permissions(_directory, fs::perms::owner_all);
                        _path = _directory / "settings";
                    } catch (...) {
                        std::error_code ignored;
                        fs::remove(_directory, ignored);
                        throw;
                    }
                    return;
                }
            }
            throw std::runtime_error("Cannot reserve configuration staging directory.");
        }
        ~staging_file() {
            std::error_code ignored;
            fs::remove(_path, ignored);
            fs::remove(_directory, ignored);
        }
        const fs::path &path() const { return _path; }
        staging_file(const staging_file &) = delete;
        staging_file &operator=(const staging_file &) = delete;
    private:
        fs::path _directory;
        fs::path _path;
    };
}

namespace native::detail
{
    configuration_file_backend::configuration_file_backend(
        std::filesystem::path path, std::vector<std::filesystem::path> defaults)
        : _path(std::move(path)), _defaults(std::move(defaults)) {}

    configuration_entries configuration_file_backend::load(
        const std::filesystem::path &path) {
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            if (!std::filesystem::exists(path))
                return {};
            throw std::runtime_error("Cannot open configuration: " + path.string());
        }
        std::vector<char> bytes;
        std::array<char, 4096> chunk{};
        while (input) {
            input.read(chunk.data(), chunk.size());
            const auto count = static_cast<std::size_t>(input.gcount());
            if (count > configuration_file_limit - bytes.size())
                throw std::runtime_error("Configuration file exceeds one MiB.");
            bytes.insert(bytes.end(), chunk.begin(), chunk.begin() + count);
        }
        if (!input.eof() || input.bad())
            throw std::runtime_error("Cannot read configuration: " + path.string());
        auto entries = decode(bytes);
        if (entries.size() > 1024)
            throw std::runtime_error("Too many configuration keys.");
        for (const auto &[key, value] : entries) {
            validate_configuration_name(key);
            validate_configuration_value(value);
        }
        return entries;
    }

    std::optional<configuration_value> configuration_file_backend::read(
        const std::string &key) {
        auto entries = load(_path);
        if (auto found = entries.find(key); found != entries.end())
            return found->second;
        for (const auto &path : _defaults) {
            entries = load(path);
            if (auto found = entries.find(key); found != entries.end())
                return found->second;
        }
        return std::nullopt;
    }

    void configuration_file_backend::write(const std::string &key,
        const std::optional<configuration_value> &value) {
        namespace fs = std::filesystem;
        const auto parent = _path.parent_path();
        if (!value && !fs::exists(_path))
            return;
        if (fs::create_directories(parent))
            fs::permissions(parent, fs::perms::owner_all);
        writer_lock lock(parent / "settings.lock");
        auto entries = load(_path);
        if (value)
            entries.insert_or_assign(key, *value);
        else if (!entries.erase(key))
            return;
        if (entries.size() > 1024)
            throw std::runtime_error("Too many configuration keys.");
        const auto bytes = encode(entries);
        if (bytes.size() > configuration_file_limit)
            throw std::runtime_error("Configuration file exceeds one MiB.");
        staging_file stage(parent);
        std::ofstream output(stage.path(), std::ios::binary | std::ios::trunc);
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        output.close();
        if (!output)
            throw std::runtime_error("Cannot write configuration: " + _path.string());
        fs::permissions(stage.path(), fs::perms::owner_read | fs::perms::owner_write);
        fs::rename(stage.path(), _path);
    }
}
