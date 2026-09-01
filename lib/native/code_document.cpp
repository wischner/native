//
// Implements canonical UTF-8 source storage, file translation, line
// indexing, overlay remapping, and document-local undo and redo.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "code_document.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <utility>

#include "text_util.h"

namespace
{
    constexpr char replacement[] = "\xef\xbf\xbd";

    bool continuation(std::uint8_t value) {
        return (value & 0xc0U) == 0x80U;
    }

    std::size_t valid_sequence_length(const std::string &text,
                                      std::size_t offset) {
        const std::uint8_t first =
            static_cast<std::uint8_t>(text[offset]);
        if (first < 0x80U)
            return first == 0 ? 0 : 1;
        std::size_t length = 0;
        std::uint32_t value = 0;
        std::uint32_t minimum = 0;
        if ((first & 0xe0U) == 0xc0U) {
            length = 2;
            value = first & 0x1fU;
            minimum = 0x80U;
        } else if ((first & 0xf0U) == 0xe0U) {
            length = 3;
            value = first & 0x0fU;
            minimum = 0x800U;
        } else if ((first & 0xf8U) == 0xf0U) {
            length = 4;
            value = first & 0x07U;
            minimum = 0x10000U;
        } else {
            return 0;
        }
        if (offset + length > text.size())
            return 0;
        for (std::size_t index = 1; index < length; ++index) {
            const std::uint8_t byte =
                static_cast<std::uint8_t>(text[offset + index]);
            if (!continuation(byte))
                return 0;
            value = (value << 6U) | (byte & 0x3fU);
        }
        if (value < minimum || value > 0x10ffffU ||
            (value >= 0xd800U && value <= 0xdfffU)) {
            return 0;
        }
        return length;
    }

    std::string repair_utf8(const std::string &text, bool &warning) {
        std::string result;
        result.reserve(text.size());
        std::size_t offset = 0;
        while (offset < text.size()) {
            const std::size_t length =
                valid_sequence_length(text, offset);
            if (length == 0) {
                result.append(replacement);
                ++offset;
                warning = true;
            } else {
                result.append(text, offset, length);
                offset += length;
            }
        }
        return result;
    }

    native::line_ending detect_ending(const std::string &text) {
        std::size_t lf = 0;
        std::size_t crlf = 0;
        std::size_t cr = 0;
        for (std::size_t index = 0; index < text.size(); ++index) {
            if (text[index] == '\r') {
                if (index + 1 < text.size() && text[index + 1] == '\n') {
                    ++crlf;
                    ++index;
                } else {
                    ++cr;
                }
            } else if (text[index] == '\n') {
                ++lf;
            }
        }
        if (crlf >= lf && crlf >= cr && crlf != 0)
            return native::line_ending::crlf;
        if (cr >= lf && cr != 0)
            return native::line_ending::cr;
        return native::line_ending::lf;
    }

    std::string canonical_lines(const std::string &text) {
        std::string result;
        result.reserve(text.size());
        for (std::size_t index = 0; index < text.size(); ++index) {
            if (text[index] == '\r') {
                if (index + 1 < text.size() && text[index + 1] == '\n')
                    ++index;
                result.push_back('\n');
            } else {
                result.push_back(text[index]);
            }
        }
        return result;
    }

    std::size_t mapped_position(std::size_t position,
                                native::text_span span,
                                std::size_t inserted,
                                bool right_affinity) {
        if (position < span.start)
            return position;
        if (position > span.end ||
            (position == span.end && span.start != span.end)) {
            return position - (span.end - span.start) + inserted;
        }
        if (position == span.start && span.start == span.end)
            return right_affinity ? span.start + inserted : span.start;
        return right_affinity ? span.start + inserted : span.start;
    }

    bool marker_less(const native::line_marker &left,
                     const native::line_marker &right) {
        if (left.line != right.line)
            return left.line < right.line;
        return static_cast<int>(left.kind) <
               static_cast<int>(right.kind);
    }
} // namespace

namespace native::detail
{
    code_document::code_document(std::string text)
        : _text(std::move(text)) {
        if (!valid_utf8(_text) ||
            _text.find('\0') != std::string::npos ||
            _text.find('\r') != std::string::npos) {
            throw std::invalid_argument(
                "code document requires canonical UTF-8 text");
        }
        rebuild_lines();
    }

    const std::string &code_document::text() const { return _text; }

    void code_document::set_text(const std::string &text) {
        if (!valid_utf8(text) ||
            text.find('\0') != std::string::npos ||
            text.find('\r') != std::string::npos) {
            throw std::invalid_argument(
                "code document requires canonical UTF-8 text");
        }
        _text = text;
        _markers.clear();
        _diagnostics.clear();
        _styles.clear();
        _undo.clear();
        _redo.clear();
        _load_warning = false;
        _loaded_bom = false;
        rebuild_lines();
    }

    void code_document::load(const std::string &path) {
        std::ifstream input(path, std::ios::binary);
        if (!input)
            throw std::runtime_error("unable to open source file");
        std::string bytes((std::istreambuf_iterator<char>(input)),
                          std::istreambuf_iterator<char>());
        if (!input.eof() && input.fail())
            throw std::runtime_error("unable to read source file");
        _loaded_bom = bytes.size() >= 3 &&
                      static_cast<std::uint8_t>(bytes[0]) == 0xefU &&
                      static_cast<std::uint8_t>(bytes[1]) == 0xbbU &&
                      static_cast<std::uint8_t>(bytes[2]) == 0xbfU;
        if (_loaded_bom)
            bytes.erase(0, 3);
        _ending = detect_ending(bytes);
        _load_warning = false;
        _text = canonical_lines(repair_utf8(bytes, _load_warning));
        _markers.clear();
        _diagnostics.clear();
        _styles.clear();
        _undo.clear();
        _redo.clear();
        rebuild_lines();
    }

    void code_document::save(const std::string &path) const {
        if (path.empty())
            throw std::invalid_argument("source path is empty");
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        if (!output)
            throw std::runtime_error("unable to open source file for write");
        if (_loaded_bom && _preserve_bom)
            output.write("\xef\xbb\xbf", 3);
        if (_ending == line_ending::lf) {
            output.write(_text.data(),
                         static_cast<std::streamsize>(_text.size()));
        } else {
            const char *sequence = _ending == line_ending::crlf
                                       ? "\r\n"
                                       : "\r";
            const std::streamsize length =
                _ending == line_ending::crlf ? 2 : 1;
            std::size_t begin = 0;
            while (begin < _text.size()) {
                const std::size_t newline = _text.find('\n', begin);
                const std::size_t end = newline == std::string::npos
                                            ? _text.size()
                                            : newline;
                output.write(_text.data() + begin,
                             static_cast<std::streamsize>(end - begin));
                if (newline == std::string::npos)
                    break;
                output.write(sequence, length);
                begin = newline + 1;
            }
        }
        if (!output)
            throw std::runtime_error("unable to write source file");
    }

    line_ending code_document::ending() const { return _ending; }

    void code_document::set_ending(line_ending value) {
        _ending = value;
    }

    bool code_document::load_warning() const { return _load_warning; }

    bool code_document::preserve_bom() const { return _preserve_bom; }

    void code_document::set_preserve_bom(bool preserve) {
        _preserve_bom = preserve;
    }

    int code_document::line_count() const {
        return static_cast<int>(_line_starts.size());
    }

    int code_document::line_at(std::size_t offset) const {
        if (offset > _text.size())
            throw std::out_of_range("source offset is out of range");
        const auto found = std::upper_bound(
            _line_starts.begin(), _line_starts.end(), offset);
        return static_cast<int>(
            std::distance(_line_starts.begin(), found) - 1);
    }

    std::size_t code_document::line_start(int line) const {
        if (line < 0 || line >= line_count())
            throw std::out_of_range("source line is out of range");
        return _line_starts[static_cast<std::size_t>(line)];
    }

    std::size_t code_document::line_end(int line) const {
        const std::size_t start = line_start(line);
        const std::size_t next = line + 1 < line_count()
                                     ? line_start(line + 1)
                                     : _text.size();
        return next > start && _text[next - 1] == '\n'
                   ? next - 1
                   : next;
    }

    std::string code_document::line_text(int line) const {
        const std::size_t start = line_start(line);
        return _text.substr(start, line_end(line) - start);
    }

    void code_document::insert(std::size_t offset,
                               const std::string &text) {
        replace(text_span{offset, offset}, text);
    }

    void code_document::erase(text_span span) {
        replace(span, std::string());
    }

    void code_document::replace(text_span span,
                                const std::string &text) {
        replace_impl(span, text, true);
    }

    bool code_document::can_undo() const { return !_undo.empty(); }

    bool code_document::can_redo() const { return !_redo.empty(); }

    bool code_document::undo() {
        if (_undo.empty())
            return false;
        edit_record record = std::move(_undo.back());
        _undo.pop_back();
        replace_impl(
            text_span{record.start,
                      record.start + record.inserted.size()},
            record.removed,
            false);
        _markers = record.markers_before;
        _diagnostics = record.diagnostics_before;
        _styles = record.styles_before;
        _redo.push_back(std::move(record));
        return true;
    }

    bool code_document::redo() {
        if (_redo.empty())
            return false;
        edit_record record = std::move(_redo.back());
        _redo.pop_back();
        replace_impl(
            text_span{record.start,
                      record.start + record.removed.size()},
            record.inserted,
            false);
        _markers = record.markers_after;
        _diagnostics = record.diagnostics_after;
        _styles = record.styles_after;
        _undo.push_back(std::move(record));
        return true;
    }

    void code_document::add_marker(line_marker marker) {
        if (marker.line < 0 || marker.line >= line_count())
            throw std::out_of_range("marker line is out of range");
        const auto same = [marker](const line_marker &item) {
            return item.line == marker.line && item.kind == marker.kind;
        };
        if (std::find_if(_markers.begin(), _markers.end(), same) !=
            _markers.end()) {
            return;
        }
        _markers.push_back(marker);
        std::sort(_markers.begin(), _markers.end(), marker_less);
    }

    void code_document::remove_marker(int line, marker_kind kind) {
        _markers.erase(
            std::remove_if(
                _markers.begin(), _markers.end(),
                [line, kind](const line_marker &item) {
                    return item.line == line && item.kind == kind;
                }),
            _markers.end());
    }

    void code_document::clear_markers(marker_kind kind) {
        _markers.erase(
            std::remove_if(
                _markers.begin(), _markers.end(),
                [kind](const line_marker &item) {
                    return item.kind == kind;
                }),
            _markers.end());
    }

    const std::vector<line_marker> &code_document::markers() const {
        return _markers;
    }

    void code_document::set_diagnostics(
        std::vector<diagnostic> items) {
        for (const diagnostic &item : items)
            validate_span(item.span);
        std::sort(items.begin(), items.end(),
                  [](const diagnostic &left,
                     const diagnostic &right) {
                      return left.span.start < right.span.start;
                  });
        _diagnostics = std::move(items);
    }

    const std::vector<diagnostic> &
    code_document::diagnostics() const {
        return _diagnostics;
    }

    void code_document::set_style_runs(std::vector<style_run> runs) {
        std::sort(runs.begin(), runs.end(),
                  [](const style_run &left, const style_run &right) {
                      return left.span.start < right.span.start;
                  });
        std::size_t previous_end = 0;
        for (const style_run &run : runs) {
            validate_span(run.span);
            if (run.span.start < previous_end)
                throw std::invalid_argument(
                    "source style runs overlap");
            previous_end = run.span.end;
        }
        _styles = std::move(runs);
    }

    const std::vector<style_run> &code_document::style_runs() const {
        return _styles;
    }

    bool code_document::valid_offset(std::size_t offset) const {
        return offset <= _text.size() &&
               (offset == _text.size() ||
                !continuation(static_cast<std::uint8_t>(_text[offset])));
    }

    void code_document::rebuild_lines() {
        _line_starts.clear();
        _line_starts.push_back(0);
        for (std::size_t index = 0; index < _text.size(); ++index) {
            if (_text[index] == '\n')
                _line_starts.push_back(index + 1);
        }
        if (_line_starts.size() >
            static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            throw std::length_error("source has too many lines");
        }
    }

    void code_document::replace_impl(text_span span,
                                     const std::string &text,
                                     bool record) {
        validate_span(span);
        if (!valid_utf8(text) || text.find('\0') != std::string::npos ||
            text.find('\r') != std::string::npos) {
            throw std::invalid_argument(
                "source edit requires canonical UTF-8 text");
        }
        if (span.start == span.end && text.empty())
            return;
        edit_record item;
        if (record) {
            item.start = span.start;
            item.removed = _text.substr(span.start,
                                        span.end - span.start);
            item.inserted = text;
            item.markers_before = _markers;
            item.diagnostics_before = _diagnostics;
            item.styles_before = _styles;
        }
        remap_overlays(span, text);
        _text.replace(span.start, span.end - span.start, text);
        rebuild_lines();
        if (record) {
            item.markers_after = _markers;
            item.diagnostics_after = _diagnostics;
            item.styles_after = _styles;
            _undo.push_back(std::move(item));
            _redo.clear();
        }
    }

    void code_document::remap_overlays(
        text_span old_span,
        const std::string &replacement_text) {
        struct marked_offset
        {
            std::size_t offset;
            marker_kind kind;
        };
        std::vector<marked_offset> mapped;
        for (const line_marker &marker : _markers) {
            const std::size_t offset = line_start(marker.line);
            if (old_span.start != old_span.end &&
                offset >= old_span.start && offset < old_span.end) {
                continue;
            }
            mapped.push_back(marked_offset{
                mapped_position(offset,
                                old_span,
                                replacement_text.size(),
                                true),
                marker.kind});
        }

        const auto map_diagnostic = [&](text_span span) {
            return text_span{
                mapped_position(span.start,
                                old_span,
                                replacement_text.size(),
                                false),
                mapped_position(span.end,
                                old_span,
                                replacement_text.size(),
                                true)};
        };
        for (diagnostic &item : _diagnostics)
            item.span = map_diagnostic(item.span);
        for (style_run &run : _styles) {
            run.span = text_span{
                mapped_position(run.span.start,
                                old_span,
                                replacement_text.size(),
                                true),
                mapped_position(run.span.end,
                                old_span,
                                replacement_text.size(),
                                false)};
        }

        std::string future = _text;
        future.replace(old_span.start,
                       old_span.end - old_span.start,
                       replacement_text);
        std::vector<std::size_t> future_lines{0};
        for (std::size_t index = 0; index < future.size(); ++index) {
            if (future[index] == '\n')
                future_lines.push_back(index + 1);
        }
        _markers.clear();
        for (const marked_offset &marker : mapped) {
            const auto found = std::upper_bound(
                future_lines.begin(), future_lines.end(), marker.offset);
            const int line = static_cast<int>(
                std::distance(future_lines.begin(), found) - 1);
            _markers.push_back(line_marker{line, marker.kind});
        }
        std::sort(_markers.begin(), _markers.end(), marker_less);
        _markers.erase(
            std::unique(
                _markers.begin(), _markers.end(),
                [](const line_marker &left, const line_marker &right) {
                    return left.line == right.line &&
                           left.kind == right.kind;
                }),
            _markers.end());

        _diagnostics.erase(
            std::remove_if(
                _diagnostics.begin(), _diagnostics.end(),
                [](const diagnostic &item) {
                    return item.span.start >= item.span.end;
                }),
            _diagnostics.end());
        _styles.erase(
            std::remove_if(
                _styles.begin(), _styles.end(),
                [](const style_run &run) {
                    return run.span.start >= run.span.end;
                }),
            _styles.end());
    }

    void code_document::validate_span(text_span span) const {
        if (span.start > span.end || span.end > _text.size() ||
            !valid_offset(span.start) || !valid_offset(span.end)) {
            throw std::out_of_range(
                "source span is not on valid UTF-8 boundaries");
        }
    }
} // namespace native::detail
