//
// Checks the GEMix desktop program icon: encoding against the SDK's
// resgen output, first-root publication, no replacement, and silent
// failure. No AES or gemd connection is needed.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

#include <native.h>

#include "../lib/native/toolkits/gemix/program_icon.h"

namespace
{
    namespace fs = std::filesystem;
    int failures = 0;

    void expect(bool condition, const char *message) {
        if (!condition) {
            std::cerr << "FAILED: " << message << '\n';
            ++failures;
        }
    }

    std::vector<std::uint8_t> read(const fs::path &path) {
        std::ifstream file(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(file),
                std::istreambuf_iterator<char>()};
    }
} // namespace

int program(int argc, char **argv) {
    if (argc != 4) {
        std::cerr << "usage: " << argv[0]
                  << " ICON.PNG RESGEN.RSC SCRATCH_DIR\n";
        return 2;
    }
    const native::img icon = native::img::load(argv[1]);
    const auto encoded =
        native::detail::encode_gem_program_icon(icon, "GEAR");
    expect(encoded == read(argv[2]),
           "Native encodes the same icon resource as SDK resgen");

    const fs::path scratch = argv[3];
    std::error_code error;
    fs::remove_all(scratch, error);
    const fs::path root = scratch / "resources";
    setenv("GEM_RESOURCE_DIR", root.c_str(), 1);
    const fs::path target =
        root / "icons" / "native_gemix_program_icon_tests.rsc";
    native::detail::publish_gem_program_icon(icon);
    const auto published = read(target);
    expect(published == native::detail::encode_gem_program_icon(
                            icon, "NATIVE_GEMIX"),
           "a missing icon is written to GEM_RESOURCE_DIR/icons");

    native::img other(8, 8);
    native::detail::publish_gem_program_icon(other);
    expect(read(target) == published,
           "an installed program icon is never replaced");

    std::ofstream(scratch / "file") << "not a directory";
    setenv("GEM_RESOURCE_DIR", (scratch / "file" / "root").c_str(), 1);
    native::detail::publish_gem_program_icon(icon);
    expect(!fs::exists(scratch / "file" / "root", error),
           "an unwritable root fails silently");

    unsetenv("GEM_RESOURCE_DIR");
    setenv("XDG_DATA_HOME", (scratch / "data").c_str(), 1);
    native::detail::publish_gem_program_icon(icon);
    expect(fs::exists(scratch / "data" / "gem" / "icons" /
                          "native_gemix_program_icon_tests.rsc",
                      error),
           "without an override the user data folder is used");

    fs::remove_all(scratch, error);
    if (!failures)
        std::cout << "GEMix program icon checks passed\n";
    return failures ? 1 : 0;
}
