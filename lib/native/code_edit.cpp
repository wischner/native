//
// Implements portable source-editor state, editing commands, overlays,
// file operations, pointer routing, and completion interaction.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/code_edit.h>

#include <algorithm>
#include <stdexcept>
#include <utility>

#include <native/clipboard.h>
#include <native/font.h>

#include "code_document.h"
#include "code_render.h"
#include "text_util.h"

namespace
{
    constexpr int marker_width = 18;

    native::rgba selected_color(native::rgba preferred,
                                native::rgba fallback) {
        return preferred.a == 0 ? fallback : preferred;
    }

    native::rgba diagnostic_color(
        const native::code_theme &value,
        native::diagnostic_severity severity) {
        if (severity == native::diagnostic_severity::hint)
            return selected_color(
                value.diagnostic_hint,
                native::rgba(95, 125, 135, 255));
        if (severity == native::diagnostic_severity::info)
            return selected_color(
                value.diagnostic_info,
                native::rgba(35, 105, 180, 255));
        if (severity == native::diagnostic_severity::warning)
            return selected_color(
                value.diagnostic_warning,
                native::rgba(205, 125, 0, 255));
        return selected_color(
            value.diagnostic_error,
            native::rgba(190, 35, 35, 255));
    }

    int digit_count(int value) {
        int result = 1;
        while (value >= 10) {
            value /= 10;
            ++result;
        }
        return result;
    }

    int line_height() {
        const native::font_metrics metrics =
            native::font_t::stock(native::font_role::fixed)
                .get_metrics();
        return std::max(1, metrics.height + 2);
    }

    int gutter_width(const native::code_edit &editor) {
        if (!editor.get_show_line_numbers())
            return marker_width;
        const native::font_metrics metrics =
            native::font_t::stock(native::font_role::fixed)
                .get_metrics();
        return marker_width + 8 +
               digit_count(editor.line_count()) *
                   std::max(1, metrics.max_advance);
    }

    std::size_t offset_for_column(const std::string &text,
                                  std::size_t begin,
                                  std::size_t end,
                                  int column) {
        std::size_t result = begin;
        for (int index = 0; index < column && result < end; ++index)
            result = native::detail::next_utf8(text, result);
        return result;
    }
} // namespace

namespace native
{
    code_edit::code_edit(std::string text,
                         coord x,
                         coord y,
                         dim width,
                         dim height)
        : text_edit(text,
                    text_edit_mode::multi_line,
                    x,
                    y,
                    width,
                    height)
        , _document(std::make_unique<detail::code_document>(
              std::move(text))) {
        on_wnd_paint.connect([this](wnd_paint_event event) {
            draw_code_edit(*this, event.g);
            return true;
        });
        on_mouse_click.connect([this](mouse_event event) {
            if (event.button != mouse_button::left ||
                event.action != mouse_action::press) {
                return false;
            }
            handle_click(event.position);
            return true;
        });
        on_mouse_wheel.connect([this](mouse_wheel_event event) {
            handle_wheel(event);
            return true;
        });
        on_mouse_move.connect([this](point position) {
            handle_hover(position);
            return true;
        });
    }

    code_edit::code_edit(const std::string &text,
                         const point &position,
                         const size &dimensions)
        : code_edit(text,
                    position.x,
                    position.y,
                    dimensions.w,
                    dimensions.h) {}

    code_edit::code_edit(const std::string &text, const rect &bounds)
        : code_edit(text, bounds.p, bounds.d) {}

    code_edit::~code_edit() { destroy(); }

    code_edit &code_edit::set_text(const std::string &utf8) {
        if (!validate(utf8))
            throw std::invalid_argument(
                "code editor value was rejected by validation");
        if (_document->text() == utf8)
            return *this;
        _document->set_text(utf8);
        _text = utf8;
        _caret = std::min(_caret, utf8.size());
        _anchor = _caret;
        _first_visible_line = 0;
        restyle(0, utf8.size());
        invalidate();
        return *this;
    }

    const std::string &code_edit::get_text() const {
        return _document->text();
    }

    code_edit &code_edit::set_path(const std::string &path) {
        _path = path;
        return *this;
    }

    const std::string &code_edit::get_path() const { return _path; }

    code_edit &code_edit::load() {
        if (_path.empty())
            throw std::invalid_argument("source path is empty");
        auto loaded = std::make_unique<detail::code_document>();
        loaded->set_preserve_bom(_document->preserve_bom());
        loaded->load(_path);
        if (!validate(loaded->text()))
            throw std::invalid_argument(
                "loaded source was rejected by validation");
        _document = std::move(loaded);
        _text = _document->text();
        _caret = 0;
        _anchor = 0;
        _first_visible_line = 0;
        restyle(0, _text.size());
        invalidate();
        return *this;
    }

    code_edit &code_edit::load(const std::string &path) {
        set_path(path);
        return load();
    }

    const code_edit &code_edit::save() const {
        if (_path.empty())
            throw std::invalid_argument("source path is empty");
        _document->save(_path);
        return *this;
    }

    code_edit &code_edit::save_as(const std::string &path) {
        set_path(path);
        save();
        return *this;
    }

    bool code_edit::get_load_warning() const {
        return _document->load_warning();
    }

    code_edit &code_edit::set_preserve_bom(bool preserve) {
        _document->set_preserve_bom(preserve);
        return *this;
    }

    bool code_edit::get_preserve_bom() const {
        return _document->preserve_bom();
    }

    line_ending code_edit::get_line_ending() const {
        return _document->ending();
    }

    code_edit &code_edit::set_line_ending(line_ending ending) {
        _document->set_ending(ending);
        return *this;
    }

    int code_edit::line_count() const {
        return _document->line_count();
    }

    int code_edit::line_at(std::size_t byte_offset) const {
        return _document->line_at(byte_offset);
    }

    std::size_t code_edit::line_start(int line) const {
        return _document->line_start(line);
    }

    std::string code_edit::line_text(int line) const {
        return _document->line_text(line);
    }

    code_edit &code_edit::go_to_line(int line) {
        return go_to_offset(line_start(line));
    }

    code_edit &code_edit::go_to_offset(std::size_t byte_offset) {
        if (!_document->valid_offset(byte_offset))
            throw std::out_of_range(
                "caret is not on a valid UTF-8 boundary");
        _caret = byte_offset;
        _anchor = byte_offset;
        _preferred_column = -1;
        reveal_caret();
        invalidate();
        return *this;
    }

    std::size_t code_edit::get_caret_offset() const { return _caret; }

    code_edit &code_edit::set_show_line_numbers(bool show) {
        _show_line_numbers = show;
        invalidate();
        return *this;
    }

    bool code_edit::get_show_line_numbers() const {
        return _show_line_numbers;
    }

    code_edit &code_edit::set_tab_width(int columns) {
        if (columns <= 0)
            throw std::invalid_argument("tab width must be positive");
        _tab_width = columns;
        invalidate();
        return *this;
    }

    int code_edit::get_tab_width() const { return _tab_width; }

    code_edit &code_edit::set_language(
        const std::string &language_id) {
        _language = language_id;
        invalidate();
        return *this;
    }

    const std::string &code_edit::get_language() const {
        return _language;
    }

    code_edit &code_edit::set_lexer(code_lexer *lexer) {
        _lexer = lexer;
        if (_lexer && _language.empty())
            _language = _lexer->language_id();
        restyle(0, _document->text().size());
        invalidate();
        return *this;
    }

    code_lexer *code_edit::get_lexer() const { return _lexer; }

    code_edit &code_edit::set_code_theme(code_theme value) {
        _code_theme = std::move(value);
        invalidate();
        return *this;
    }

    const code_theme &code_edit::get_code_theme() const {
        return _code_theme;
    }

    void code_edit::add_marker(line_marker marker) {
        _document->add_marker(marker);
        invalidate();
    }

    void code_edit::remove_marker(int line, marker_kind kind) {
        _document->remove_marker(line, kind);
        invalidate();
    }

    void code_edit::clear_markers(marker_kind kind) {
        _document->clear_markers(kind);
        invalidate();
    }

    std::vector<line_marker> code_edit::markers() const {
        return _document->markers();
    }

    void code_edit::set_diagnostics(std::vector<diagnostic> items) {
        _document->set_diagnostics(std::move(items));
        invalidate();
    }

    const std::vector<diagnostic> &code_edit::diagnostics() const {
        return _document->diagnostics();
    }

    void code_edit::set_style_runs(std::vector<style_run> runs) {
        _document->set_style_runs(std::move(runs));
        invalidate();
    }

    const std::vector<style_run> &code_edit::style_runs() const {
        return _document->style_runs();
    }

    void code_edit::show_completion(
        std::vector<completion_item> items) {
        _completion = std::move(items);
        _completion_visible = !_completion.empty();
        _completion_index = _completion_visible ? 0 : -1;
        invalidate();
    }

    void code_edit::hide_completion() {
        _completion_visible = false;
        _completion_index = -1;
        _completion.clear();
        invalidate();
    }

    bool code_edit::get_completion_visible() const {
        return _completion_visible;
    }

    code_edit &code_edit::insert(std::size_t offset,
                                 const std::string &utf8) {
        return replace(text_span{offset, offset}, utf8);
    }

    code_edit &code_edit::erase(text_span span) {
        return replace(span, std::string());
    }

    code_edit &code_edit::replace(text_span span,
                                  const std::string &utf8) {
        if (span.start > span.end ||
            span.end > _document->text().size() ||
            !_document->valid_offset(span.start) ||
            !_document->valid_offset(span.end)) {
            throw std::out_of_range(
                "source span is not on valid UTF-8 boundaries");
        }
        const std::string proposed =
            _document->text().substr(0, span.start) + utf8 +
            _document->text().substr(span.end);
        if (!validate(proposed))
            throw std::invalid_argument(
                "source edit was rejected by validation");
        if (proposed == _document->text())
            return *this;
        _document->replace(span, utf8);
        edited(span.start + utf8.size());
        restyle(span.start, span.start + utf8.size());
        return *this;
    }

    bool code_edit::can_undo() const { return _document->can_undo(); }

    bool code_edit::can_redo() const { return _document->can_redo(); }

    bool code_edit::undo() {
        if (!_document->undo())
            return false;
        if (!validate(_document->text())) {
            _document->redo();
            return false;
        }
        edited(std::min(_caret, _document->text().size()));
        restyle(0, _document->text().size());
        return true;
    }

    bool code_edit::redo() {
        if (!_document->redo())
            return false;
        if (!validate(_document->text())) {
            _document->undo();
            return false;
        }
        edited(std::min(_caret, _document->text().size()));
        restyle(0, _document->text().size());
        return true;
    }

    bool code_edit::get_read_only() const { return _read_only; }

    code_edit &code_edit::set_read_only(bool read_only) {
        _read_only = read_only;
        apply_read_only();
        return *this;
    }

    bool code_edit::copy() const {
        const std::string selected = selected_text();
        if (selected.empty())
            return false;
        clipboard output = clipboard::open_write();
        output.write_text(selected).commit();
        return true;
    }

    bool code_edit::cut() {
        if (_read_only || !copy())
            return false;
        return replace_selected_text(std::string());
    }

    bool code_edit::paste() {
        if (_read_only)
            return false;
        clipboard input = clipboard::open_read();
        if (!input.has(clipboard_format::text))
            return false;
        return replace_selected_text(input.read_text());
    }

    void code_edit::select_all() const { select_all_native(); }

    bool code_edit::on_native_key(code_edit_key key, bool extend) {
        if (_completion_visible) {
            if (key == code_edit_key::up || key == code_edit_key::down) {
                const int delta = key == code_edit_key::up ? -1 : 1;
                _completion_index = std::clamp(
                    _completion_index + delta,
                    0,
                    static_cast<int>(_completion.size()) - 1);
                invalidate();
                return true;
            }
            if (key == code_edit_key::enter) {
                const completion_item item =
                    _completion[static_cast<std::size_t>(
                        _completion_index)];
                hide_completion();
                on_native_complete(item);
                return true;
            }
            if (key == code_edit_key::escape) {
                hide_completion();
                return true;
            }
        }

        if (key == code_edit_key::copy)
            return copy();
        if (key == code_edit_key::cut)
            return cut();
        if (key == code_edit_key::paste)
            return paste();
        if (key == code_edit_key::select_all) {
            select_all();
            return true;
        }
        if (key == code_edit_key::undo)
            return !_read_only && undo();
        if (key == code_edit_key::redo)
            return !_read_only && redo();

        const std::string &text = _document->text();
        std::size_t target = _caret;
        const int current_line = _document->line_at(_caret);
        if (key == code_edit_key::left) {
            target = detail::previous_utf8(text, _caret);
            _preferred_column = -1;
        } else if (key == code_edit_key::right) {
            target = detail::next_utf8(text, _caret);
            _preferred_column = -1;
        } else if (key == code_edit_key::home) {
            target = _document->line_start(current_line);
            _preferred_column = -1;
        } else if (key == code_edit_key::end) {
            target = _document->line_end(current_line);
            _preferred_column = -1;
        } else if (key == code_edit_key::up ||
                   key == code_edit_key::down ||
                   key == code_edit_key::page_up ||
                   key == code_edit_key::page_down) {
            if (_preferred_column < 0) {
                std::size_t scan = _document->line_start(current_line);
                _preferred_column = 0;
                while (scan < _caret) {
                    scan = detail::next_utf8(text, scan);
                    ++_preferred_column;
                }
            }
            int distance = 1;
            if (key == code_edit_key::page_up ||
                key == code_edit_key::page_down) {
                distance = std::max(
                    1,
                    static_cast<int>(get_dimensions().h) /
                        line_height());
            }
            const bool upward = key == code_edit_key::up ||
                                key == code_edit_key::page_up;
            const int line = std::clamp(
                current_line + (upward ? -distance : distance),
                0,
                line_count() - 1);
            target = offset_for_column(
                text,
                _document->line_start(line),
                _document->line_end(line),
                _preferred_column);
        } else if (key == code_edit_key::backspace) {
            if (_read_only)
                return false;
            if (_caret != _anchor)
                return replace_selected_text(std::string());
            if (_caret == 0)
                return true;
            erase(text_span{detail::previous_utf8(text, _caret),
                            _caret});
            return true;
        } else if (key == code_edit_key::delete_forward) {
            if (_read_only)
                return false;
            if (_caret != _anchor)
                return replace_selected_text(std::string());
            if (_caret == text.size())
                return true;
            erase(text_span{_caret,
                            detail::next_utf8(text, _caret)});
            return true;
        } else if (key == code_edit_key::enter) {
            return !_read_only &&
                   replace_selected_text(std::string("\n"));
        } else if (key == code_edit_key::tab) {
            return !_read_only &&
                   replace_selected_text(std::string("\t"));
        } else if (key == code_edit_key::escape) {
            return false;
        } else {
            return false;
        }

        _caret = target;
        if (!extend)
            _anchor = _caret;
        reveal_caret();
        invalidate();
        return true;
    }

    bool code_edit::on_native_text_input(const std::string &utf8) {
        if (_read_only || utf8.empty())
            return false;
        return replace_selected_text(utf8);
    }

    bool code_edit::on_native_text(const std::string &text) {
        if (_read_only || !validate(text))
            return false;
        if (_document->text() == text)
            return true;
        _document->replace(
            text_span{0, _document->text().size()}, text);
        edited(std::min(_caret, text.size()));
        restyle(0, text.size());
        return true;
    }

    void code_edit::on_native_focus(bool focused) {
        if (_focused == focused)
            return;
        _focused = focused;
        invalidate();
    }

    bool code_edit::get_focused() const { return _focused; }

    void code_edit::apply_text() { invalidate(); }

    void code_edit::apply_read_only() { invalidate(); }

    std::string code_edit::selected_text() const {
        return _document->text().substr(
            ordered_selection_start(),
            ordered_selection_end() - ordered_selection_start());
    }

    bool code_edit::replace_selected_text(const std::string &text) {
        if (_read_only)
            return false;
        const text_span selection{ordered_selection_start(),
                                  ordered_selection_end()};
        const std::string proposed =
            _document->text().substr(0, selection.start) + text +
            _document->text().substr(selection.end);
        if (!validate(proposed))
            return false;
        replace(selection, text);
        return true;
    }

    void code_edit::select_all_native() const {
        auto *self = const_cast<code_edit *>(this);
        self->_anchor = 0;
        self->_caret = _document->text().size();
        self->reveal_caret();
        self->invalidate();
    }

    void code_edit::on_bounds_changed() {
        reveal_caret();
        invalidate();
    }

    void code_edit::edited(std::size_t caret) {
        _text = _document->text();
        _caret = std::min(caret, _text.size());
        _anchor = _caret;
        _preferred_column = -1;
        reveal_caret();
        invalidate();
        on_change.emit(_text);
        on_text_change.emit();
    }

    void code_edit::restyle(std::size_t start, std::size_t end) {
        if (!_lexer)
            return;
        try {
            const std::size_t size = _document->text().size();
            const int first_line = _document->line_at(
                std::min(start, size));
            const int last_line = _document->line_at(
                std::min(end, size));
            const std::size_t dirty_start =
                _document->line_start(first_line);
            const std::size_t dirty_end =
                last_line + 1 < _document->line_count()
                    ? _document->line_start(last_line + 1)
                    : size;
            std::vector<style_run> changed = _lexer->lex(
                _document->text(), dirty_start, dirty_end);
            const bool complete = std::any_of(
                changed.begin(),
                changed.end(),
                [dirty_start, dirty_end](const style_run &run) {
                    return run.span.start < dirty_start ||
                           run.span.end > dirty_end;
                });
            if (complete) {
                _document->set_style_runs(std::move(changed));
                return;
            }
            std::vector<style_run> merged;
            for (const style_run &run : _document->style_runs()) {
                if (run.span.end <= dirty_start ||
                    run.span.start >= dirty_end) {
                    merged.push_back(run);
                }
            }
            merged.insert(merged.end(),
                          changed.begin(),
                          changed.end());
            _document->set_style_runs(std::move(merged));
        } catch (...) {
            std::vector<style_run> fallback;
            if (!_document->text().empty()) {
                fallback.push_back(style_run{
                    text_span{0, _document->text().size()}, 0});
            }
            _document->set_style_runs(std::move(fallback));
        }
    }

    void code_edit::reveal_caret() {
        const int caret_line = _document->line_at(
            std::min(_caret, _document->text().size()));
        const int visible = std::max(
            1,
            static_cast<int>(get_dimensions().h) / line_height());
        if (caret_line < _first_visible_line)
            _first_visible_line = caret_line;
        else if (caret_line >= _first_visible_line + visible)
            _first_visible_line = caret_line - visible + 1;
        _first_visible_line = std::clamp(
            _first_visible_line, 0, std::max(0, line_count() - 1));
    }

    std::size_t code_edit::ordered_selection_start() const {
        return std::min(_caret, _anchor);
    }

    std::size_t code_edit::ordered_selection_end() const {
        return std::max(_caret, _anchor);
    }

    void code_edit::handle_click(point position) {
        const int height = line_height();
        const int line = std::clamp(
            _first_visible_line +
                std::max(0, static_cast<int>(position.y)) / height,
            0,
            line_count() - 1);
        if (position.x < marker_width) {
            on_native_gutter_click(line);
            return;
        }
        const std::string text = _document->line_text(line);
        const int relative_x = std::max(
            0,
            static_cast<int>(position.x) - gutter_width(*this) - 5);
        const font_t &font = font_t::stock(font_role::fixed);
        std::size_t local = 0;
        int width = 0;
        int column = 0;
        while (local < text.size()) {
            const std::size_t next = detail::next_utf8(text, local);
            int advance = 0;
            if (text[local] == '\t') {
                const int spaces = _tab_width - column % _tab_width;
                advance = font.measure_text(
                    std::string(static_cast<std::size_t>(spaces), ' '))
                              .advance;
                column += spaces;
            } else {
                advance = font.measure_text(
                    text.substr(local, next - local)).advance;
                ++column;
            }
            if (relative_x < width + advance / 2)
                break;
            width += advance;
            local = next;
        }
        _caret = _document->line_start(line) + local;
        _anchor = _caret;
        _preferred_column = -1;
        invalidate();
    }

    void code_edit::handle_wheel(mouse_wheel_event event) {
        if (event.direction != wheel_direction::vertical)
            return;
        const int lines = std::max(
            1, std::abs(static_cast<int>(event.delta)) / 24);
        _first_visible_line += event.delta > 0 ? -lines : lines;
        const int visible = std::max(
            1,
            static_cast<int>(get_dimensions().h) / line_height());
        _first_visible_line = std::clamp(
            _first_visible_line,
            0,
            std::max(0, line_count() - visible));
        invalidate();
    }

    void code_edit::handle_hover(point position) {
        const int line = _first_visible_line +
            std::max(0, static_cast<int>(position.y)) / line_height();
        if (line < 0 || line >= line_count())
            return;
        const std::size_t start = _document->line_start(line);
        const std::size_t end = _document->line_end(line);
        on_native_hover(text_span{start, end});
    }

    void code_edit::on_native_complete(
        const completion_item &item) {
        on_complete.emit(item);
    }

    void code_edit::on_native_gutter_click(int line) {
        on_gutter_click.emit(line);
    }

    void code_edit::on_native_hover(text_span span) {
        on_hover.emit(span);
    }

    void code_edit::draw_editor_background(
        gpx &,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_surface(bounds, surface_kind::inset, state);
    }

    void code_edit::draw_gutter(
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &) {
        const theme::palette palette = appearance.native_palette();
        graphics.set_ink(selected_color(
            _code_theme.gutter_background, palette.button_bg))
            .draw_rect(bounds, true);
        appearance.draw_separator(
            rect(static_cast<coord>(bounds.x2() - 1),
                 bounds.p.y,
                 1,
                 bounds.d.h),
            separator_orientation::vertical);
    }

    void code_edit::draw_line_background(
        gpx &graphics,
        theme &appearance,
        int,
        const rect &bounds,
        const theme::state &state) {
        if (_code_theme.current_line.a != 0) {
            graphics.set_ink(_code_theme.current_line)
                .draw_rect(bounds, true);
            return;
        }
        appearance.draw_selection(
            bounds, selection_shape::row, state);
    }

    void code_edit::draw_line_number(
        gpx &graphics,
        theme &appearance,
        int line,
        const rect &bounds,
        const theme::state &) {
        graphics.set_font(font_t::stock(font_role::fixed))
            .set_ink(selected_color(
                _code_theme.gutter_text,
                appearance.native_palette().button_disabled_text))
            .draw_text(
                std::to_string(line + 1),
                bounds,
                text_layout{text_align::end,
                            text_valign::center,
                            text_overflow::clip,
                            true});
    }

    void code_edit::draw_marker(
        gpx &graphics,
        theme &appearance,
        const line_marker &marker,
        const rect &bounds,
        const theme::state &state) {
        const rgba color = selected_color(
            _code_theme.marker, rgba(190, 35, 35, 255));
        graphics.set_ink(color).set_pen(1);
        if (marker.kind == marker_kind::breakpoint) {
            graphics.draw_ellipse(bounds, true);
        } else if (marker.kind == marker_kind::breakpoint_disabled) {
            graphics.draw_ellipse(bounds, false);
        } else if (marker.kind == marker_kind::current_line) {
            graphics.draw_polygon(
                {point(bounds.p.x, bounds.p.y),
                 point(bounds.x2(),
                       static_cast<coord>(
                           bounds.p.y + bounds.d.h / 2)),
                 point(bounds.p.x, bounds.y2())},
                true);
        } else if (marker.kind == marker_kind::bookmark) {
            graphics.draw_rect(bounds, true);
        } else {
            appearance.draw_disclosure(
                bounds,
                marker.kind == marker_kind::fold_closed
                    ? disclosure_state::collapsed
                    : disclosure_state::expanded,
                state);
        }
    }

    void code_edit::draw_style_background(
        gpx &graphics,
        theme &,
        const style_run &,
        const code_style &style,
        const rect &bounds,
        const theme::state &) {
        if (style.background.a != 0)
            graphics.set_ink(style.background).draw_rect(bounds, true);
    }

    void code_edit::draw_selection(
        gpx &,
        theme &appearance,
        const text_span &,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_selection(
            bounds, selection_shape::row, state);
    }

    void code_edit::draw_text_content(
        gpx &graphics,
        theme &,
        const text_span &,
        const std::string &display,
        point position,
        rgba foreground,
        bool bold,
        const theme::state &) {
        graphics.set_font(font_t::stock(font_role::fixed))
            .set_ink(foreground)
            .draw_text(display, position);
        if (bold) {
            graphics.draw_text(
                display,
                point(static_cast<coord>(position.x + 1),
                      position.y));
        }
    }

    void code_edit::draw_diagnostic(
        gpx &graphics,
        theme &,
        const diagnostic &item,
        const rect &bounds,
        const theme::state &) {
        const coord y = static_cast<coord>(bounds.y2() - 1);
        graphics.set_ink(diagnostic_color(
            _code_theme, item.severity)).draw_line(
                point(bounds.p.x, y), point(bounds.x2(), y));
    }

    void code_edit::draw_caret(
        gpx &graphics,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        const theme::palette colors = appearance.native_palette();
        graphics.set_ink(state.selected ? colors.selection_text
                                        : colors.content_text)
            .draw_line(bounds.p,
                       point(bounds.p.x,
                             static_cast<coord>(bounds.y2() - 1)));
    }

    void code_edit::draw_editor_focus(
        gpx &,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_focus(bounds, state);
    }

    void code_edit::draw_completion_background(
        gpx &,
        theme &appearance,
        const rect &bounds,
        const theme::state &state) {
        appearance.draw_surface(bounds, surface_kind::popup, state);
    }

    void code_edit::draw_completion_item(
        gpx &,
        theme &appearance,
        std::size_t,
        const completion_item &item,
        const rect &bounds,
        const theme::state &state) {
        std::string label = item.label;
        if (!item.detail.empty())
            label += "  " + item.detail;
        appearance.draw_list_item(bounds, label, state);
    }
} // namespace native
