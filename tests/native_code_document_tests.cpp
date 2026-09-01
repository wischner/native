//
// Verifies source-document UTF-8, line, overlay, file, and undo rules.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

#include "code_document.h"

namespace
{
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    std::filesystem::path temporary_file(const char *suffix) {
        const auto stamp = std::chrono::steady_clock::now()
                               .time_since_epoch()
                               .count();
        return std::filesystem::temp_directory_path() /
               (std::string("native-code-document-") +
                std::to_string(stamp) + suffix);
    }

    std::string read_file(const std::filesystem::path &path) {
        std::ifstream input(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(input),
                           std::istreambuf_iterator<char>());
    }

    void test_lines_and_edits() {
        native::detail::code_document document("one\ntwo\nthree");
        require(document.line_count() == 3, "line count");
        require(document.line_start(1) == 4, "line start");
        require(document.line_at(7) == 1, "line lookup");
        require(document.line_text(2) == "three", "line text");

        document.insert(3, "!");
        require(document.text() == "one!\ntwo\nthree", "insert");
        document.erase(native::text_span{3, 4});
        require(document.text() == "one\ntwo\nthree", "erase");
        document.replace(native::text_span{4, 7}, "second");
        require(document.text() == "one\nsecond\nthree", "replace");
        require(document.can_undo(), "undo available");
        require(document.undo(), "undo succeeds");
        require(document.text() == "one\ntwo\nthree", "undo value");
        require(document.redo(), "redo succeeds");
        require(document.text() == "one\nsecond\nthree", "redo value");
    }

    void test_marker_mapping() {
        native::detail::code_document document("one\ntwo\nthree");
        document.add_marker(
            native::line_marker{1, native::marker_kind::breakpoint});
        document.insert(document.line_start(1), "\n");
        require(document.markers().size() == 1 &&
                    document.markers()[0].line == 2,
                "newline before marker moves it");

        document.set_text("one\ntwo\nthree");
        document.add_marker(
            native::line_marker{1, native::marker_kind::breakpoint});
        document.insert(document.line_start(1) + 1, "\n");
        require(document.markers()[0].line == 1,
                "split line keeps marker");

        document.set_text("one\ntwo\nthree");
        document.add_marker(
            native::line_marker{1, native::marker_kind::breakpoint});
        document.erase(native::text_span{document.line_start(1),
                                         document.line_start(2)});
        require(document.markers().empty(),
                "deleting marked line removes marker");
    }

    void test_styles_and_boundaries() {
        native::detail::code_document document("a\xc3\xa9z");
        require(!document.valid_offset(2), "continuation boundary");
        bool rejected = false;
        try {
            document.insert(2, "x");
        } catch (const std::out_of_range &) {
            rejected = true;
        }
        require(rejected, "invalid boundary rejected");
        document.set_style_runs(
            {native::style_run{native::text_span{0, 1}, 1},
             native::style_run{native::text_span{1, 3}, 2}});
        require(document.style_runs().size() == 2,
                "style runs retained");
        document.insert(1, "x");
        require(document.style_runs()[0].span.end == 1 &&
                    document.style_runs()[1].span.start == 2,
                "insertion between styles does not overlap runs");
    }

    void test_file_translation() {
        const std::filesystem::path input_path =
            temporary_file("-input.txt");
        const std::filesystem::path output_path =
            temporary_file("-output.txt");
        {
            std::ofstream output(input_path,
                                 std::ios::binary | std::ios::trunc);
            const char bytes[] = {
                static_cast<char>(0xef),
                static_cast<char>(0xbb),
                static_cast<char>(0xbf),
                'a', '\r', '\n', static_cast<char>(0xff), '\r', '\n'};
            output.write(bytes, sizeof(bytes));
        }

        native::detail::code_document document;
        document.load(input_path.string());
        require(document.ending() == native::line_ending::crlf,
                "CRLF detected");
        require(document.load_warning(), "invalid UTF-8 warning");
        require(document.text() == "a\n\xef\xbf\xbd\n",
                "loaded text canonicalized");
        document.save(output_path.string());
        require(read_file(output_path) ==
                    std::string("\xef\xbb\xbf") +
                        "a\r\n\xef\xbf\xbd\r\n",
                "save preserves BOM and CRLF");

        std::filesystem::remove(input_path);
        std::filesystem::remove(output_path);
    }
} // namespace

int main() {
    test_lines_and_edits();
    test_marker_mapping();
    test_styles_and_boundaries();
    test_file_translation();
    return 0;
}
