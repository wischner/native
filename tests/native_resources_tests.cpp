//
// Exercises portable resource ownership, variant ranking, binary rejection,
// manifest validation and file round trips without opening a display.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include <native/resources.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
    void check(bool condition) {
        if (!condition) throw std::runtime_error("Resource test failed.");
    }
    template<class exception, class function>
    void rejects(function run) {
        try { run(); }
        catch (const exception &) { return; }
        throw std::runtime_error("Expected resource rejection.");
    }
    void write(const std::filesystem::path &path, const std::string &text) {
        std::ofstream out(path, std::ios::binary);
        out << text;
        out.close();
        check(bool(out));
    }
    void run(const std::filesystem::path &directory) {
        using native::resource_context;
        using native::resource_variant;
        using native::resources;
        resources pack;
        const std::string name = "assets.icons.platform-icon";
        pack.add({name, "icon", {}, {0, 255, 1}});
        pack.add({name, "icon", {"", "", "monochrome", "", 16}, {16}});
        pack.add({name, "icon", {"linux", "x11", "monochrome", "", 32}, {32}});
        pack.add({name, "icon", {"windows", "", "color", "", 16}, {99}});
        pack.add({"empty", "blob", {}, {}});
        check(pack.get(name, {}).data[0] == 0);
        check(pack.get(name, {"linux", "x11", "monochrome", "", 16}).data[0] == 32);
        check(pack.get(name, {"windows", "", "monochrome", "", 16}).data[0] == 16);
        check(pack.get(name, {"windows", "", "color", "", 16}).data[0] == 99);
        check(pack.get(name, {"haiku", "", "color", "", 16}).data[0] == 0);
        check(pack.get(name).data == pack.get(name, resources::current_context()).data);
        const auto current = resources::current_context();
        check(!current.platform.empty());
        if (current.platform == "linux") {
            check(!current.toolkit.empty());
            check(current.color == ((current.toolkit == "x11" ||
                current.toolkit == "gemix") ? "monochrome" : "color"));
        }
        rejects<std::out_of_range>([&] { (void)pack.get("missing"); });
        rejects<std::invalid_argument>([&] { pack.add({name, "text", {}, {}}); });
        rejects<std::invalid_argument>([&] { pack.add({name, "icon", {}, {}}); });
        rejects<std::invalid_argument>([&] { pack.add({"bad name", "blob", {}, {}}); });
        rejects<std::invalid_argument>([&] { pack.add({"bad", "blob", {"", "", "blue", "", 0}, {}}); });
        resources sizes;
        sizes.add({"size", "icon", {}, {0}});
        sizes.add({"size", "icon", {"", "", "", "", 16}, {16}});
        sizes.add({"size", "icon", {"", "", "", "", 32}, {32}});
        check(sizes.get("size", {"", "", "", "", 24}).data[0] == 32);
        check(sizes.get("size", {"", "", "", "", 17}).data[0] == 16);
        check(sizes.get("size", {}).data[0] == 0);
        sizes.add({"size", "icon", {"", "", "", "dark", 16}, {42}});
        check(sizes.get("size", {"", "", "", "dark", 16}).data[0] == 42);
        resources incomplete;
        incomplete.add({"x", "blob", {"windows", "", "", "", 0}, {}});
        rejects<std::invalid_argument>([&] { (void)incomplete.encode(); });
        rejects<std::out_of_range>([&] { (void)incomplete.get("x", {"linux", "", "", "", 0}); });
        const auto encoded = pack.encode();
        check(encoded[0] == 'N' && encoded[8] == 1 && encoded[12] == 5);
        check(resources::decode(encoded).encode() == encoded);
        check(resources::decode(resources{}.encode()).entries().empty());
        for (std::size_t size = 0; size < encoded.size(); ++size)
            rejects<std::invalid_argument>([&] {
                (void)resources::decode(std::span(encoded).first(size));
            });
        auto bad = encoded;
        bad.push_back(0);
        rejects<std::invalid_argument>([&] { (void)resources::decode(bad); });
        for (std::size_t offset : {0, 8, 12, 16}) {
            bad = encoded;
            bad[offset] = 255;
            rejects<std::invalid_argument>([&] { (void)resources::decode(bad); });
        }
        auto owned = resources::decode(encoded);
        auto copied = owned;
        owned.add({"extra", "blob", {}, {1}});
        check(copied.entries().size() == 5);
        const auto file = directory / "test.resource";
        pack.save(file);
        check(resources::load(file).encode() == encoded);
        rejects<std::invalid_argument>([&] { incomplete.save(file); });
        check(resources::load(file).encode() == encoded);
        rejects<std::runtime_error>([&] { pack.save(directory / "missing" / "pack"); });
        rejects<std::runtime_error>([&] { (void)resources::load(directory / "absent"); });
        const auto manifest = directory / "manifest.json";
        write(directory / "asset.txt", "hello");
        const std::string valid = R"({"version":1,"resources":[{"name":"greeting","type":"text","variants":[{"file":"asset.txt"}]}]})";
        write(manifest, valid);
        auto compiled = resources::from_manifest(manifest);
        check(compiled.get("greeting").type == "text");
        check(compiled.get("greeting").data.size() == 5);
        check(resources::decode(compiled.encode()).encode() == compiled.encode());
        for (const std::string &text : {
            std::string("{}"), std::string("[]"), valid + "x",
            std::string(R"({"version":1,"version":1,"resources":[]})"),
            std::string(R"({"version":01,"resources":[]})"),
            std::string(R"({"version":4294967296,"resources":[]})"),
            std::string(R"({"version":1,"resources":[],"typo":1})"),
            std::string(R"({"version":1,"resources":[],})"),
            std::string(R"({"version":1,"resources":[{"name":"x","type":"blob","variants":[]}]})"),
            std::string(R"({"version":1,"resources":[{"name":"x","type":"blob","variants":[{"file":"asset.txt","when":{"size":16}}]}]})")}) {
            write(manifest, text);
            rejects<std::invalid_argument>([&] { (void)resources::from_manifest(manifest); });
        }
        for (const std::string token : {R"(\ud800)", R"(\udc00)", R"(\u0000)", "\xc0\xaf"}) {
            auto text = valid;
            text.replace(text.find("asset.txt"), 9, token);
            write(manifest, text);
            rejects<std::invalid_argument>([&] { (void)resources::from_manifest(manifest); });
        }
        write(directory / std::filesystem::path(u8"\u00e9\U0001f600.txt"), "unicode");
        auto unicode = valid;
        unicode.replace(unicode.find("asset.txt"), 9, R"(\u00e9\ud83d\ude00.txt)");
        write(manifest, unicode);
        check(resources::from_manifest(manifest).get("greeting").data.size() == 7);
        write(manifest, std::string(17, '[') + std::string(17, ']'));
        rejects<std::invalid_argument>([&] { (void)resources::from_manifest(manifest); });
    }
}
int main(int argc, char **argv) {
    try {
        check(argc == 2);
        const std::filesystem::path directory(argv[1]);
        std::filesystem::create_directories(directory);
        run(directory);
        std::cout << "Resource tests passed.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
