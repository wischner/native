//
// Declares the private portable source document used by code_edit.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <native/code_edit.h>

namespace native::detail
{
    // Owns canonical UTF-8 source, indexes, overlays, and undo steps.
    class code_document
    {
    public:
        explicit code_document(std::string text = {});

        const std::string &text() const;
        void set_text(const std::string &text);

        void load(const std::string &path);
        void save(const std::string &path) const;

        line_ending ending() const;
        void set_ending(line_ending value);

        bool load_warning() const;
        bool preserve_bom() const;
        void set_preserve_bom(bool preserve);

        int line_count() const;
        int line_at(std::size_t offset) const;
        std::size_t line_start(int line) const;
        std::size_t line_end(int line) const;
        std::string line_text(int line) const;

        void insert(std::size_t offset, const std::string &text);
        void erase(text_span span);
        void replace(text_span span, const std::string &text);

        bool can_undo() const;
        bool can_redo() const;
        bool undo();
        bool redo();

        void add_marker(line_marker marker);
        void remove_marker(int line, marker_kind kind);
        void clear_markers(marker_kind kind);
        const std::vector<line_marker> &markers() const;

        void set_diagnostics(std::vector<diagnostic> items);
        const std::vector<diagnostic> &diagnostics() const;

        void set_style_runs(std::vector<style_run> runs);
        const std::vector<style_run> &style_runs() const;

        bool valid_offset(std::size_t offset) const;

    private:
        struct edit_record
        {
            std::size_t start = 0;
            std::string removed;
            std::string inserted;
            std::vector<line_marker> markers_before;
            std::vector<line_marker> markers_after;
            std::vector<diagnostic> diagnostics_before;
            std::vector<diagnostic> diagnostics_after;
            std::vector<style_run> styles_before;
            std::vector<style_run> styles_after;
        };

        std::string _text;
        std::vector<std::size_t> _line_starts;
        std::vector<line_marker> _markers;
        std::vector<diagnostic> _diagnostics;
        std::vector<style_run> _styles;
        std::vector<edit_record> _undo;
        std::vector<edit_record> _redo;
        line_ending _ending = line_ending::lf;
        bool _load_warning = false;
        bool _loaded_bom = false;
        bool _preserve_bom = true;

        void rebuild_lines();
        void replace_impl(text_span span,
                          const std::string &text,
                          bool record);
        void remap_overlays(text_span old_span,
                            const std::string &replacement);
        void validate_span(text_span span) const;
    };
} // namespace native::detail
