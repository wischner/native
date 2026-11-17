//
// Exercises actual per-platform preference stores without a UI: native
// value representation, process persistence, concurrent key merging and
// cache visibility. Test namespaces are unique and removed on exit.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/configuration.h>
#include <native/app.h>
#include <native/process.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <CoreFoundation/CoreFoundation.h>
#elif defined(__HAIKU__)
#include <FindDirectory.h>
#include <Path.h>
#include <DataIO.h>
#include <Message.h>
#endif

namespace
{
    namespace fs = std::filesystem;
    using namespace native;
    using namespace std::chrono_literals;

    void expect(bool condition, const char *message) {
        if (!condition) throw std::runtime_error(message);
    }

    template<typename Work>
    void rejects(Work work) {
        bool caught = false;
        try { work(); } catch (const std::exception &) { caught = true; }
        expect(caught, "invalid settings operation must fail explicitly");
    }

#if !defined(_WIN32) && !defined(__APPLE__)
    std::string read_file(const fs::path &path) {
        std::ifstream input(path, std::ios::binary);
        expect(static_cast<bool>(input), "open native settings file");
        return {std::istreambuf_iterator<char>(input), {}};
    }

    void write_file(const fs::path &path, const std::string &text) {
        fs::create_directories(path.parent_path());
        std::ofstream output(path, std::ios::binary);
        output << text;
        output.close();
        expect(static_cast<bool>(output), "write test settings fixture");
    }

#endif

    struct fixture
    {
        std::string application;
        fs::path directory;
        fs::path settings;

        fixture() {
            std::random_device random;
            application = "org.native.configuration-test-" +
                std::to_string(random()) + "-" + std::to_string(random()) + " Mail";
            directory = fs::temp_directory_path() / application;
            expect(fs::create_directory(directory), "reserve test namespace");
#if defined(_WIN32)
            HKEY existing = nullptr;
            const std::wstring path = L"Software\\" + std::wstring(application.begin(), application.end());
            const auto status = RegOpenKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, KEY_READ, &existing);
            if (existing) RegCloseKey(existing);
            expect(status == ERROR_FILE_NOT_FOUND, "registry namespace is unused");
#elif defined(__HAIKU__)
            BPath root;
            expect(find_directory(B_USER_SETTINGS_DIRECTORY, &root, false) == B_OK,
                   "find Haiku user settings");
            settings = fs::path(root.Path()) / application / "settings";
            expect(!fs::exists(settings.parent_path()), "Haiku test namespace is unused");
#elif !defined(__APPLE__)
            const auto home = directory / "home";
            const auto user = directory / "user";
            const auto system = directory / "system";
            expect(setenv("HOME", home.c_str(), 1) == 0 &&
                   setenv("XDG_CONFIG_HOME", user.c_str(), 1) == 0 &&
                   setenv("XDG_CONFIG_DIRS", system.c_str(), 1) == 0,
                   "isolate XDG test directories");
            settings = user / application / "settings.ini";
#endif
        }

        ~fixture() {
            std::error_code ignored;
#if defined(_WIN32)
            const std::wstring path = L"Software\\" + std::wstring(application.begin(), application.end());
            RegDeleteTreeW(HKEY_CURRENT_USER, path.c_str());
#elif defined(__APPLE__)
            CFStringRef domain = CFStringCreateWithCString(nullptr, application.c_str(), kCFStringEncodingUTF8);
            CFArrayRef keys = CFPreferencesCopyKeyList(domain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
            if (keys) {
                for (CFIndex i = 0; i < CFArrayGetCount(keys); ++i)
                    CFPreferencesSetValue(static_cast<CFStringRef>(CFArrayGetValueAtIndex(keys, i)),
                        nullptr, domain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
                CFRelease(keys);
            }
            CFPreferencesSynchronize(domain, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
            CFRelease(domain);
#elif defined(__HAIKU__)
            fs::remove_all(settings.parent_path(), ignored);
#endif
            fs::remove_all(directory, ignored);
        }
    };

    void native_representation(const fixture &data) {
#if defined(_WIN32)
        const std::wstring path = L"Software\\" +
            std::wstring(data.application.begin(), data.application.end()) + L"\\db";
        DWORD type = 0, size = 0;
        expect(RegGetValueW(HKEY_CURRENT_USER, path.c_str(), L"port", RRF_RT_ANY,
                   &type, nullptr, &size) == ERROR_SUCCESS && type == REG_QWORD && size == 8,
               "integer is a native registry QWORD");
#elif defined(__APPLE__)
        CFStringRef domain = CFStringCreateWithCString(nullptr, data.application.c_str(), kCFStringEncodingUTF8);
        CFTypeRef value = CFPreferencesCopyValue(CFSTR("db.port"), domain,
            kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
        const bool native_number = value && CFGetTypeID(value) == CFNumberGetTypeID() &&
            !CFNumberIsFloatType(static_cast<CFNumberRef>(value));
        if (value) CFRelease(value);
        CFRelease(domain);
        expect(native_number, "integer is a native macOS preference number");
#elif defined(__HAIKU__)
        const auto bytes = read_file(data.settings);
        BMemoryIO input(bytes.data(), bytes.size());
        BMessage message;
        int64 port = 0;
        expect(message.Unflatten(&input) == B_OK &&
               message.FindInt64("db.port", &port) == B_OK && port == 5432,
               "Haiku settings are a native flattened BMessage");
#else
        const auto text = read_file(data.settings);
        expect(text.find("db.port = 5432\n") != std::string::npos &&
               text.find("db.name = \"") != std::string::npos,
               "XDG settings are readable text at the standard location");
#endif
    }

    void children(const std::string &application, const fs::path &executable) {
        std::vector<std::unique_ptr<process>> running;
        for (int index = 0; index < 4; ++index) {
            process_config config;
            config.executable = executable;
            config.arguments = {"--writer", application, std::to_string(index)};
            auto child = std::make_unique<process>(config);
            expect(child->start(), "launch independent configuration writer");
            running.push_back(std::move(child));
        }
        for (auto &child : running) {
            child->wait();
            const auto result = child->get_result();
            if (result.exit_code != 0 || result.reason != process_exit::exited)
                throw std::runtime_error("Configuration child failed: " + result.error + result.launch_error);
        }
        app::set_name(application);
        for (int writer = 0; writer < 4; ++writer) {
            for (int key = 0; key < 8; ++key) {
                configuration<int> value(
                    "writer" + std::to_string(writer) + ".key" + std::to_string(key), -1);
                expect(value.get_value() == writer * 100 + key,
                       "concurrent writers preserve every independent key");
            }
        }
    }

    void checks(const fs::path &executable) {
        fixture data;
        app::set_name(data.application);
        configuration name("db.name", "default");
        configuration<int> port("db.port", 1234);
        expect(name.get_value() == "default" && port.get_value() == 1234,
               "missing native preferences use defaults");
        if (!data.settings.empty())
            expect(!fs::exists(data.settings.parent_path()), "reads do not create configuration files");
        const std::string text = "Mail café 日本語 \\\"\n\r\t";
        name = text;
        port = 5432;
        configuration<bool> flag("ui.enabled", false);
        configuration<double> scale("ui.scale", 0.0);
        configuration<long long> large("db.large", 0LL);
        flag = true;
        scale = 1.234567890123456;
        large = (std::numeric_limits<long long>::min)();
        expect(name.reload() == text && flag.reload() && scale.reload() == 1.234567890123456 &&
               large.reload() == (std::numeric_limits<long long>::min)(),
               "native storage round-trips all scalar types");
        scale = std::numeric_limits<double>::denorm_min();
        expect(scale.reload() == std::numeric_limits<double>::denorm_min(),
               "smallest subnormal number round-trips");
        name = std::string("control\b\f\x01");
        expect(name.reload() == std::string("control\b\f\x01"),
               "control characters round-trip without corrupting the file");
        name = text;
        native_representation(data);
        children(data.application, executable);
        expect(name.reload() == text && port.reload() == 5432,
               "other process writes preserve existing values");
        configuration<int> cached("db.port", 0, 1h);
        expect(cached.get_value() == 5432, "cache initially loads from platform");
        port = 7000;
        expect(cached.get_value() == 5432 && cached.reload() == 7000,
               "cache and forced refresh have distinct storage behavior");
        cached.set_timeout(5ms);
        expect(cached.get_value() == 7000, "timeout change reloads from platform");
        port = 7100;
        std::this_thread::sleep_for(20ms);
        expect(cached.get_value() == 7100, "expired cache observes a separate binding's write");
        configuration wrong("db.port", "wrong type");
        rejects([&] { wrong.get_value(); });
        name.reset();
        expect(name.get_value() == "default", "reset removes a native user override");

#if !defined(_WIN32) && !defined(__APPLE__)
        const auto original = read_file(data.settings);
        write_file(data.settings, "not a valid settings file\n");
        rejects([&] { port.reload(); });
        rejects([&] { port = 9000; });
        expect(read_file(data.settings) == "not a valid settings file\n",
               "failed writes never overwrite corrupt settings");
        write_file(data.settings, original);
#if !defined(__HAIKU__)
        fs::permissions(data.settings.parent_path(), fs::perms::owner_read | fs::perms::owner_exec);
        rejects([&] { port = 9000; });
        expect(read_file(data.settings) == original, "publication failure preserves the previous file");
        fs::permissions(data.settings.parent_path(), fs::perms::owner_all);
#endif
#endif

#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__HAIKU__)
        const auto system_file = data.directory / "system" / data.application / "settings.ini";
        write_file(system_file, "db.port = 9000\nsystem.only = true\n");
        expect(port.get_value() == 7100, "user settings override XDG system defaults");
        port.reset();
        expect(port.get_value() == 9000, "reset reveals XDG system defaults");
        configuration<bool> system_only("system.only", false);
        expect(system_only.get_value(), "XDG system-only keys are readable");
        expect(setenv("XDG_CONFIG_HOME", "relative-is-invalid", 1) == 0,
               "set relative XDG test path");
        app::set_name(data.application + "-home");
        configuration<int> home_value("value", 0);
        home_value = 42;
        expect(fs::exists(data.directory / "home/.config" /
                   (data.application + "-home") / "settings.ini"),
               "relative XDG_CONFIG_HOME falls back to absolute HOME");
#endif
    }
}

int main(int argc, char **argv) {
    try {
        native::app::argc = argc;
        native::app::argv = argv;
        expect(native::app::get_name() == fs::path(argv[0]).filename().string(),
               "custom entry point gets the executable filename by default");
        if (argc == 4 && std::string(argv[1]) == "--writer") {
            const int writer = std::stoi(argv[3]);
            native::app::set_name(argv[2]);
            for (int key = 0; key < 8; ++key) {
                native::configuration<int> value(
                    "writer" + std::to_string(writer) + ".key" + std::to_string(key));
                value = writer * 100 + key;
            }
            return 0;
        }
        checks(std::filesystem::absolute(argv[0]));
        std::cout << "Native configuration storage checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
