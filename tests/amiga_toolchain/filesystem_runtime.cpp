//
// Qualifies standard streams and filesystem publication on an Amiga volume.
// Run with a writable scratch directory; each run uses its own subdirectory.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

int main(int argc, char **argv) {
    namespace fs = std::filesystem;
    const fs::path root = argc > 1 ? argv[1] : "RAM:";
    const auto stamp = std::chrono::steady_clock::now()
                           .time_since_epoch().count();
    const auto directory = root / ("native-check-" + std::to_string(stamp));
    std::error_code error;
    if (!fs::create_directory(directory, error) || error) return 1;
    // Cleanup is restricted to the directory this invocation just created.
    const auto staged = directory / "staged.txt";
    const auto published = directory / "published.txt";
    bool success = false;
    try {
        {
            std::ofstream stream(staged);
            stream << "native";
            stream.close();
            if (!stream) throw std::system_error(std::io_errc::stream);
        }
        fs::rename(staged, published);
        std::ifstream stream(published);
        std::string text;
        stream >> text;
        unsigned entries = 0;
        for (const auto &entry : fs::directory_iterator(directory)) {
            if (entry.is_regular_file()) ++entries;
        }
        success = text == "native" && entries == 1 &&
                  fs::file_size(published) == 6;
    } catch (...) {}
    fs::remove_all(directory, error);
    return success && !error ? 0 : 1;
}
