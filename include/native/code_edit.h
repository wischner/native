//
// Declares the portable UTF-8 source editor, its overlay data, lexer
// contract, completion values, and backend-neutral editing commands.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "geometry.h"
#include "text_edit.h"

namespace native
{
    namespace detail
    {
        class code_document;
    }

    // Selects the line-ending sequence written at the file boundary.
    enum class line_ending
    {
        lf,
        crlf,
        cr
    };

    // Selects a semantic mark painted beside one source line.
    enum class marker_kind
    {
        breakpoint,
        breakpoint_disabled,
        current_line,
        bookmark,
        fold_closed,
        fold_open
    };

    // Selects the importance of a diagnostic overlay.
    enum class diagnostic_severity
    {
        hint,
        info,
        warning,
        error
    };

    // Describes a half-open range of UTF-8 byte offsets.
    struct text_span
    {
        std::size_t start = 0;
        std::size_t end = 0;
    };

    // Associates one half-open source span with a lexer style ID.
    struct style_run
    {
        text_span span;
        int style_id = 0;
    };

    // Associates one zero-based source line with a semantic mark.
    struct line_marker
    {
        int line = 0;
        marker_kind kind = marker_kind::breakpoint;
    };

    // Describes one compiler or language-service diagnostic.
    struct diagnostic
    {
        text_span span;
        diagnostic_severity severity = diagnostic_severity::error;
        std::string message;
    };

    // Describes one application-supplied completion choice.
    struct completion_item
    {
        std::string label;
        std::string insert;
        std::string detail;
    };

    // Maps one lexer style ID to portable presentation attributes.
    struct code_style
    {
        rgba foreground = rgba(0, 0, 0, 255);
        rgba background;
        bool bold = false;
    };

    // Stores optional source-editor colours and lexer style mappings.
    struct code_theme
    {
        rgba gutter_background;
        rgba gutter_text;
        rgba marker;
        rgba current_line;
        rgba diagnostic_hint;
        rgba diagnostic_info;
        rgba diagnostic_warning;
        rgba diagnostic_error;
        std::vector<code_style> styles;
    };

    // Supplies application-owned syntax styles without language packs.
    class code_lexer
    {
    public:
        // Destroy an application lexer through its interface.
        virtual ~code_lexer() = default;

        // Return the language identifier understood by this lexer.
        virtual std::string language_id() const = 0;

        // Return style runs for a dirty UTF-8 byte range.
        virtual std::vector<style_run> lex(
            std::string_view utf8,
            std::size_t dirty_start,
            std::size_t dirty_end) = 0;
    };

    // Selects one backend-originated source-editor key command.
    enum class code_edit_key
    {
        left,
        right,
        up,
        down,
        home,
        end,
        page_up,
        page_down,
        backspace,
        delete_forward,
        enter,
        tab,
        escape,
        copy,
        cut,
        paste,
        select_all,
        undo,
        redo
    };

    // Presents and edits a canonical UTF-8 source document.
    class code_edit : public text_edit
    {
    public:
        // Construct a source editor from text and scalar bounds.
        code_edit(std::string text = {},
                  coord x = 0,
                  coord y = 0,
                  dim width = 480,
                  dim height = 320);

        // Construct a source editor from text, position, and size.
        code_edit(const std::string &text,
                  const point &position,
                  const size &dimensions);

        // Construct a source editor from text and complete bounds.
        code_edit(const std::string &text, const rect &bounds);

        // Destroy the editor and its native host.
        ~code_edit() override;

        // Replace the complete source and clear document undo history.
        code_edit &set_text(const std::string &utf8) override;

        // Return the canonical UTF-8 source buffer.
        const std::string &get_text() const override;

        // Set the path used by load and save without accessing it.
        code_edit &set_path(const std::string &path);

        // Return the current source path.
        const std::string &get_path() const;

        // Load UTF-8 source from the current path.
        code_edit &load();

        // Load UTF-8 source and remember the supplied path.
        code_edit &load(const std::string &path);

        // Save source through the current path and line-ending policy.
        const code_edit &save() const;

        // Save source to a new path and remember it.
        code_edit &save_as(const std::string &path);

        // Return whether malformed UTF-8 was repaired by the last load.
        bool get_load_warning() const;

        // Enable or disable preservation of a loaded UTF-8 BOM.
        code_edit &set_preserve_bom(bool preserve);

        // Return whether a loaded UTF-8 BOM will be preserved on save.
        bool get_preserve_bom() const;

        // Return the line-ending sequence selected for the next save.
        line_ending get_line_ending() const;

        // Select the line-ending sequence used by the next save.
        code_edit &set_line_ending(line_ending ending);

        // Return the number of logical source lines.
        int line_count() const;

        // Return the zero-based line containing a byte offset.
        int line_at(std::size_t byte_offset) const;

        // Return the byte offset at which a line starts.
        std::size_t line_start(int line) const;

        // Return one line without its terminating newline.
        std::string line_text(int line) const;

        // Move the caret to the start of a line and reveal it.
        code_edit &go_to_line(int line);

        // Move the caret to a UTF-8 boundary and reveal it.
        code_edit &go_to_offset(std::size_t byte_offset);

        // Return the current caret byte offset.
        std::size_t get_caret_offset() const;

        // Show or hide the line-number portion of the gutter.
        code_edit &set_show_line_numbers(bool show);

        // Return whether line numbers are shown.
        bool get_show_line_numbers() const;

        // Set the positive tab display width in columns.
        code_edit &set_tab_width(int columns);

        // Return the tab display width in columns.
        int get_tab_width() const;

        // Set the application-defined language identifier.
        code_edit &set_language(const std::string &language_id);

        // Return the application-defined language identifier.
        const std::string &get_language() const;

        // Install a borrowed lexer, or null to disable automatic lexing.
        code_edit &set_lexer(code_lexer *lexer);

        // Return the currently borrowed lexer, or null.
        code_lexer *get_lexer() const;

        // Replace optional editor colours and style mappings.
        code_edit &set_code_theme(code_theme value);

        // Return the optional editor colour and style mappings.
        const code_theme &get_code_theme() const;

        // Add a marker after validating its line.
        void add_marker(line_marker marker);

        // Remove a marker of one kind from a line.
        void remove_marker(int line, marker_kind kind);

        // Remove every marker of one kind.
        void clear_markers(marker_kind kind);

        // Return all markers in line and kind order.
        std::vector<line_marker> markers() const;

        // Replace diagnostic overlays after validating their spans.
        void set_diagnostics(std::vector<diagnostic> items);

        // Return current diagnostic overlays.
        const std::vector<diagnostic> &diagnostics() const;

        // Replace sorted, non-overlapping syntax style runs.
        void set_style_runs(std::vector<style_run> runs);

        // Return current syntax style runs.
        const std::vector<style_run> &style_runs() const;

        // Show an application-provided completion overlay.
        void show_completion(std::vector<completion_item> items);

        // Dismiss the completion overlay without accepting an item.
        void hide_completion();

        // Return whether a completion overlay is visible.
        bool get_completion_visible() const;

        // Insert valid source at a UTF-8 byte boundary.
        code_edit &insert(std::size_t offset, const std::string &utf8);

        // Erase a valid half-open UTF-8 byte range.
        code_edit &erase(text_span span);

        // Replace a valid range with canonical UTF-8 source.
        code_edit &replace(text_span span, const std::string &utf8);

        // Return whether a document-local undo step is available.
        bool can_undo() const;

        // Return whether a document-local redo step is available.
        bool can_redo() const;

        // Apply one document-local undo step.
        bool undo();

        // Apply one document-local redo step.
        bool redo();

        // Return whether user edits are currently disabled.
        bool get_read_only() const override;

        // Enable or disable user edits without emitting a signal.
        code_edit &set_read_only(bool read_only) override;

        // Copy the current selection to the typed text clipboard.
        bool copy() const override;

        // Copy and erase the current selection when editable.
        bool cut() override;

        // Insert typed text from the clipboard when editable.
        bool paste() override;

        // Select the complete source buffer.
        void select_all() const override;

        // Apply a backend-originated editing/navigation command.
        virtual bool on_native_key(code_edit_key key,
                                   bool extend = false);

        // Insert backend-originated canonical UTF-8 input.
        virtual bool on_native_text_input(const std::string &utf8);

        // Accept or reject a complete backend-originated source value.
        bool on_native_text(const std::string &text) override;

        // Cache backend focus entry or departure.
        virtual void on_native_focus(bool focused);

        // Return whether the editor host has keyboard focus.
        bool get_focused() const;

        // Create the backend source-editor host.
        void create() const override;

        // Destroy the backend source-editor host.
        void destroy() const override;

        // Show the backend source-editor host.
        void show() const override;

        // Emits after a user or edit-command source mutation.
        signal<> on_text_change;

        // Emits a zero-based line for a marker-column click.
        signal<int> on_gutter_click;

        // Emits the completion item accepted from the overlay.
        signal<completion_item> on_complete;

        // Emits the source span currently under the pointer.
        signal<text_span> on_hover;

    protected:
        // Repaint after the inherited text cache changes.
        void apply_text() override;

        // Repaint after the inherited read-only state changes.
        void apply_read_only() override;

        // Return the selected source bytes.
        std::string selected_text() const override;

        // Replace the current source selection.
        bool replace_selected_text(const std::string &text) override;

        // Select the complete source buffer.
        void select_all_native() const override;

        // Keep the caret visible after a bounds change.
        void on_bounds_changed() override;

        // Apply an accepted source edit and notify interested code.
        virtual void edited(std::size_t caret);

        // Re-run the borrowed lexer for a dirty byte range.
        virtual void restyle(std::size_t start, std::size_t end);

        // Adjust cached scrolling to keep the caret visible.
        virtual void reveal_caret();

        // Handle one client-relative pointer selection action.
        virtual void handle_click(point position);

        // Handle one portable pointer-wheel action.
        virtual void handle_wheel(mouse_wheel_event event);

        // Handle one portable pointer hover action.
        virtual void handle_hover(point position);

        // Notify acceptance of one completion item.
        virtual void on_native_complete(const completion_item &item);

        // Notify one marker-column click.
        virtual void on_native_gutter_click(int line);

        // Notify one source span under the pointer.
        virtual void on_native_hover(text_span span);

        // Draw the editor's native outer surface.
        virtual void draw_editor_background(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        // Draw the marker and line-number gutter.
        virtual void draw_gutter(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        // Draw one source line's active-line background.
        virtual void draw_line_background(
            gpx &graphics,
            theme &appearance,
            int line,
            const rect &bounds,
            const theme::state &state);

        // Draw one source line number.
        virtual void draw_line_number(
            gpx &graphics,
            theme &appearance,
            int line,
            const rect &bounds,
            const theme::state &state);

        // Draw one semantic gutter marker.
        virtual void draw_marker(
            gpx &graphics,
            theme &appearance,
            const line_marker &marker,
            const rect &bounds,
            const theme::state &state);

        // Draw one syntax style's optional background.
        virtual void draw_style_background(
            gpx &graphics,
            theme &appearance,
            const style_run &run,
            const code_style &style,
            const rect &bounds,
            const theme::state &state);

        // Draw one selected source span background.
        virtual void draw_selection(
            gpx &graphics,
            theme &appearance,
            const text_span &span,
            const rect &bounds,
            const theme::state &state);

        // Draw one already expanded source-text fragment.
        virtual void draw_text_content(
            gpx &graphics,
            theme &appearance,
            const text_span &span,
            const std::string &display,
            point position,
            rgba foreground,
            bool bold,
            const theme::state &state);

        // Draw one diagnostic annotation.
        virtual void draw_diagnostic(
            gpx &graphics,
            theme &appearance,
            const diagnostic &item,
            const rect &bounds,
            const theme::state &state);

        // Draw the insertion caret.
        virtual void draw_caret(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        // Draw focus around the completed source editor.
        virtual void draw_editor_focus(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        // Draw the completion popup's outer surface.
        virtual void draw_completion_background(
            gpx &graphics,
            theme &appearance,
            const rect &bounds,
            const theme::state &state);

        // Draw one completion popup item.
        virtual void draw_completion_item(
            gpx &graphics,
            theme &appearance,
            std::size_t index,
            const completion_item &item,
            const rect &bounds,
            const theme::state &state);

    private:
        friend class detail::control_render_access;

        std::unique_ptr<detail::code_document> _document;
        std::string _path;
        std::string _language;
        code_lexer *_lexer = nullptr;
        code_theme _code_theme;
        std::vector<completion_item> _completion;
        std::size_t _caret = 0;
        std::size_t _anchor = 0;
        int _first_visible_line = 0;
        int _completion_index = -1;
        int _preferred_column = -1;
        bool _show_line_numbers = true;
        bool _completion_visible = false;
        bool _focused = false;
        int _tab_width = 4;

        std::size_t ordered_selection_start() const;
        std::size_t ordered_selection_end() const;
        friend void draw_code_edit(code_edit &, gpx &, point);
    };
} // namespace native
