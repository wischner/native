//
// Implements the shared native-look source editor painter used by
// custom and fallback backend hosts.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include "code_render.h"

#include <algorithm>
#include <string>

#include <native/code_edit.h>
#include <native/font.h>
#include <native/graphics.h>
#include <native/theme.h>

#include "code_document.h"
#include "control_render_access.h"
#include "text_util.h"

namespace
{
    native::rgba selected_color(native::rgba preferred,
                                native::rgba fallback) {
        return preferred.a == 0 ? fallback : preferred;
    }

    int digits(int value) {
        int result = 1;
        while (value >= 10) {
            value /= 10;
            ++result;
        }
        return result;
    }

    struct editor_metrics
    {
        int line_height = 1;
        int marker_width = 18;
        int gutter_width = 18;
        int text_x = 23;
        int space_width = 1;
    };

    editor_metrics metrics_for(native::code_edit &editor,
                               native::gpx &graphics) {
        graphics.set_font(
            native::font_t::stock(native::font_role::fixed));
        const native::font_metrics font = graphics.get_font_metrics();
        editor_metrics result;
        result.line_height = std::max(1, font.height + 2);
        result.space_width = std::max(
            1, graphics.measure_text(std::string(" ")).advance);
        if (editor.get_show_line_numbers()) {
            result.gutter_width += 8 +
                digits(editor.line_count()) *
                    std::max(1, font.max_advance);
        }
        result.text_x = result.gutter_width + 5;
        return result;
    }

    struct measured_text
    {
        std::string display;
        int width = 0;
        int column = 0;
    };

    measured_text measure_range(const std::string &source,
                                std::size_t begin,
                                std::size_t end,
                                int initial_column,
                                int tab_width,
                                native::gpx &graphics) {
        measured_text result;
        result.column = initial_column;
        std::size_t offset = begin;
        while (offset < end) {
            const std::size_t next =
                native::detail::next_utf8(source, offset);
            if (source[offset] == '\t') {
                const int count = tab_width - result.column % tab_width;
                result.display.append(
                    static_cast<std::size_t>(count), ' ');
                result.column += count;
            } else {
                result.display.append(source, offset, next - offset);
                ++result.column;
            }
            offset = next;
        }
        result.width = graphics.measure_text(result.display).advance;
        return result;
    }

    int column_at(const std::string &source,
                  std::size_t line_start,
                  std::size_t offset,
                  int tab_width) {
        int column = 0;
        std::size_t scan = line_start;
        while (scan < offset) {
            if (source[scan] == '\t')
                column += tab_width - column % tab_width;
            else
                ++column;
            scan = native::detail::next_utf8(source, scan);
        }
        return column;
    }

    int x_at(const std::string &source,
             std::size_t line_start,
             std::size_t offset,
             int tab_width,
             native::gpx &graphics,
             int text_x) {
        return text_x +
               measure_range(source,
                             line_start,
                             offset,
                             0,
                             tab_width,
                             graphics)
                   .width;
    }

    void draw_text_segment(native::code_edit &editor,
                           native::gpx &graphics,
                           native::theme &painter,
                           const std::string &source,
                           std::size_t line_start,
                           std::size_t begin,
                           std::size_t end,
                           int tab_width,
                           int text_x,
                           int origin_x,
                           int y,
                           native::rgba foreground,
                           bool bold,
                           native::text_span selection,
                           native::rgba selection_text) {
        const auto draw_piece = [&](std::size_t piece_begin,
                                    std::size_t piece_end,
                                    native::rgba ink,
                                    bool selected) {
            if (piece_begin >= piece_end)
                return;
            const measured_text measured = measure_range(
                source,
                piece_begin,
                piece_end,
                column_at(source,
                          line_start,
                          piece_begin,
                          tab_width),
                tab_width,
                graphics);
            const int x = origin_x + x_at(source,
                                          line_start,
                                          piece_begin,
                                          tab_width,
                                          graphics,
                                          text_x);
            native::theme::state state;
            state.selected = selected;
            state.focused = editor.get_focused();
            native::detail::control_render_access::draw_text_content(
                editor,
                graphics,
                painter,
                native::text_span{piece_begin, piece_end},
                measured.display,
                native::point(static_cast<native::coord>(x),
                              static_cast<native::coord>(y + 1)),
                ink,
                bold,
                state);
        };

        if (selection.end <= begin || selection.start >= end) {
            draw_piece(begin, end, foreground, false);
            return;
        }
        const std::size_t selected_begin =
            std::max(begin, selection.start);
        const std::size_t selected_end =
            std::min(end, selection.end);
        draw_piece(begin, selected_begin, foreground, false);
        draw_piece(
            selected_begin, selected_end, selection_text, true);
        draw_piece(
            std::max(begin, selected_end), end, foreground, false);
    }

    native::rgba style_foreground(
        const native::code_theme &code_theme,
        int style_id,
        native::rgba fallback) {
        if (style_id < 0 ||
            static_cast<std::size_t>(style_id) >=
                code_theme.styles.size()) {
            return fallback;
        }
        return selected_color(
            code_theme.styles[static_cast<std::size_t>(style_id)]
                .foreground,
            fallback);
    }

    void draw_completion(native::code_edit &editor,
                         native::gpx &graphics,
                         native::theme &painter,
                         const editor_metrics &metrics,
                         native::point origin,
                         int caret_x,
                         int caret_y,
                         const std::vector<native::completion_item> &items,
                         int selected,
                         bool visible) {
        if (!visible || items.empty())
            return;
        const int count = std::min(
            8, static_cast<int>(items.size()));
        const int first = std::clamp(
            selected - count + 1,
            0,
            std::max(0, static_cast<int>(items.size()) - count));
        const int row_height = std::max(
            metrics.line_height,
            painter.defaults().list_item_height);
        const int popup_width = std::min(
            300,
            std::max(120,
                     static_cast<int>(editor.get_dimensions().w) -
                         metrics.gutter_width));
        int x = std::clamp(
            caret_x,
            metrics.gutter_width,
            std::max(metrics.gutter_width,
                     static_cast<int>(editor.get_dimensions().w) -
                         popup_width));
        int y = caret_y + metrics.line_height;
        const int popup_height = count * row_height + 2;
        if (y + popup_height > editor.get_dimensions().h)
            y = std::max(0, caret_y - popup_height);
        native::theme::state popup_state;
        const native::rect popup_bounds(
                static_cast<native::coord>(origin.x + x),
                static_cast<native::coord>(origin.y + y),
                static_cast<native::dim>(popup_width),
                static_cast<native::dim>(popup_height));
        native::detail::control_render_access::
            draw_completion_background(
                editor,
                graphics,
                painter,
                popup_bounds,
                popup_state);
        for (int index = 0; index < count; ++index) {
            const int item_index = first + index;
            native::theme::state item_state;
            item_state.selected = item_index == selected;
            const auto semantic_index =
                static_cast<std::size_t>(item_index);
            native::detail::control_render_access::
                draw_completion_item(
                    editor,
                    graphics,
                    painter,
                    semantic_index,
                    items[semantic_index],
                    native::rect(
                    static_cast<native::coord>(origin.x + x + 1),
                    static_cast<native::coord>(origin.y + y + 1 +
                                               index * row_height),
                    static_cast<native::dim>(popup_width - 2),
                    static_cast<native::dim>(row_height)),
                    item_state);
        }
    }
} // namespace

namespace native
{
    void draw_code_edit(code_edit &editor,
                        gpx &graphics,
                        point origin) {
        auto saved = graphics.save_state();
        const editor_metrics values = metrics_for(editor, graphics);
        auto painter = theme::create(graphics);
        const theme::palette palette = painter->native_palette();
        const code_theme &colors = editor._code_theme;
        const size dimensions = editor.get_dimensions();
        const rect outer(origin, dimensions);
        graphics.set_clip(graphics.get_clip().intersect(outer));
        theme::state editor_state;
        editor_state.focused = editor._focused;
        editor_state.disabled = editor._read_only;
        editor.draw_editor_background(
            graphics, *painter, outer, editor_state);
        editor.draw_gutter(
            graphics,
            *painter,
            rect(origin.x,
                 origin.y,
                 static_cast<dim>(values.gutter_width),
                 dimensions.h),
            editor_state);

        const std::string &source = editor._document->text();
        const int visible_lines = std::max(
            1,
            (static_cast<int>(dimensions.h) + values.line_height - 1) /
                values.line_height);
        const int last_line = std::min(
            editor.line_count(),
            editor._first_visible_line + visible_lines);
        const std::size_t selection_start =
            editor.ordered_selection_start();
        const std::size_t selection_end =
            editor.ordered_selection_end();
        const int caret_line = editor.line_at(editor._caret);
        const auto &markers = editor._document->markers();
        const auto &styles = editor._document->style_runs();
        const auto &diagnostics = editor._document->diagnostics();
        std::size_t marker_index = 0;
        std::size_t style_index = 0;
        std::size_t diagnostic_index = 0;
        while (marker_index < markers.size() &&
               markers[marker_index].line <
                   editor._first_visible_line) {
            ++marker_index;
        }

        for (int line = editor._first_visible_line;
             line < last_line;
             ++line) {
            const int local_y =
                (line - editor._first_visible_line) * values.line_height;
            const int y = origin.y + local_y;
            const std::size_t begin = editor._document->line_start(line);
            const std::size_t end = editor._document->line_end(line);
            const bool native_current_line =
                line == caret_line && colors.current_line.a == 0;

            if (line == caret_line) {
                theme::state line_state = editor_state;
                line_state.selected = native_current_line;
                editor.draw_line_background(
                    graphics,
                    *painter,
                    line,
                    rect(static_cast<coord>(origin.x +
                                            values.gutter_width),
                         static_cast<coord>(y),
                         static_cast<dim>(
                             std::max(0,
                                 static_cast<int>(dimensions.w) -
                                     values.gutter_width)),
                         static_cast<dim>(values.line_height)),
                    line_state);
            }

            if (editor.get_show_line_numbers()) {
                editor.draw_line_number(
                    graphics,
                    *painter,
                    line,
                    rect(static_cast<coord>(origin.x +
                                            values.marker_width),
                         static_cast<coord>(y),
                         static_cast<dim>(values.gutter_width -
                                          values.marker_width - 4),
                         static_cast<dim>(values.line_height)),
                    editor_state);
            }

            while (marker_index < markers.size() &&
                   markers[marker_index].line < line) {
                ++marker_index;
            }
            std::size_t line_marker_index = marker_index;
            while (line_marker_index < markers.size() &&
                   markers[line_marker_index].line == line) {
                const int side = std::max(
                    5, std::min(11, values.line_height - 4));
                editor.draw_marker(
                    graphics,
                    *painter,
                    markers[line_marker_index],
                    rect(static_cast<coord>(
                             origin.x +
                             (values.marker_width - side) / 2),
                         static_cast<coord>(
                             y + (values.line_height - side) / 2),
                         static_cast<dim>(side),
                         static_cast<dim>(side)),
                    editor_state);
                ++line_marker_index;
            }
            marker_index = line_marker_index;

            while (style_index < styles.size() &&
                   styles[style_index].span.end <= begin) {
                ++style_index;
            }
            for (std::size_t index = style_index;
                 index < styles.size(); ++index) {
                const style_run &run = styles[index];
                if (run.span.start >= end)
                    break;
                if (run.span.end <= begin || run.span.start >= end ||
                    run.style_id < 0 ||
                    static_cast<std::size_t>(run.style_id) >=
                        colors.styles.size()) {
                    continue;
                }
                const code_style &style = colors.styles[
                    static_cast<std::size_t>(run.style_id)];
                if (style.background.a == 0)
                    continue;
                const std::size_t styled_begin =
                    std::max(begin, run.span.start);
                const std::size_t styled_end =
                    std::min(end, run.span.end);
                const int x1 = origin.x + x_at(
                    source,
                    begin,
                    styled_begin,
                    editor._tab_width,
                    graphics,
                    values.text_x);
                const int x2 = origin.x + x_at(
                    source,
                    begin,
                    styled_end,
                    editor._tab_width,
                    graphics,
                    values.text_x);
                editor.draw_style_background(
                    graphics,
                    *painter,
                    run,
                    style,
                    rect(static_cast<coord>(x1),
                         static_cast<coord>(y),
                         static_cast<dim>(std::max(1, x2 - x1)),
                         static_cast<dim>(values.line_height)),
                    editor_state);
            }

            const std::size_t selected_begin =
                std::max(begin, selection_start);
            const std::size_t selected_end =
                std::min(end, selection_end);
            if (selected_begin < selected_end) {
                theme::state selection_state;
                selection_state.selected = true;
                selection_state.focused = editor._focused;
                const int x1 = origin.x + x_at(
                    source,
                    begin,
                    selected_begin,
                    editor._tab_width,
                    graphics,
                    values.text_x);
                const int x2 = origin.x + x_at(
                    source,
                    begin,
                    selected_end,
                    editor._tab_width,
                    graphics,
                    values.text_x);
                editor.draw_selection(
                    graphics,
                    *painter,
                    text_span{selected_begin, selected_end},
                    rect(static_cast<coord>(x1),
                         static_cast<coord>(y),
                         static_cast<dim>(std::max(1, x2 - x1)),
                         static_cast<dim>(values.line_height)),
                    selection_state);
            }

            std::size_t offset = begin;
            const text_span selection{selection_start,
                                      selection_end};
            const rgba selected_text = editor._focused
                                           ? palette.selection_text
                                           : palette.selection_inactive_text;
            for (std::size_t index = style_index;
                 index < styles.size(); ++index) {
                const style_run &run = styles[index];
                if (run.span.start >= end)
                    break;
                if (run.span.end <= begin || run.span.start >= end)
                    continue;
                const std::size_t styled_begin =
                    std::max({offset, begin, run.span.start});
                if (offset < styled_begin) {
                    draw_text_segment(editor,
                                      graphics,
                                      *painter,
                                      source,
                                      begin,
                                      offset,
                                      styled_begin,
                                      editor._tab_width,
                                      values.text_x,
                                      origin.x,
                                      y,
                                      native_current_line
                                          ? palette.selection_text
                                          : palette.content_text,
                                      false,
                                      selection,
                                      selected_text);
                }
                const std::size_t styled_end =
                    std::min(end, run.span.end);
                if (styled_begin < styled_end) {
                    const code_style *style =
                        run.style_id >= 0 &&
                                static_cast<std::size_t>(run.style_id) <
                                    colors.styles.size()
                            ? &colors.styles[static_cast<std::size_t>(
                                  run.style_id)]
                            : nullptr;
                    draw_text_segment(
                        editor,
                        graphics,
                        *painter,
                        source,
                        begin,
                        styled_begin,
                        styled_end,
                        editor._tab_width,
                        values.text_x,
                        origin.x,
                        y,
                        native_current_line
                            ? palette.selection_text
                            : style_foreground(
                                  colors,
                                  run.style_id,
                                  palette.content_text),
                        style && style->bold,
                        selection,
                        selected_text);
                    offset = styled_end;
                }
            }
            if (offset < end) {
                draw_text_segment(editor,
                                  graphics,
                                  *painter,
                                  source,
                                  begin,
                                  offset,
                                  end,
                                  editor._tab_width,
                                  values.text_x,
                                  origin.x,
                                  y,
                                  native_current_line
                                      ? palette.selection_text
                                      : palette.content_text,
                                  false,
                                  selection,
                                  selected_text);
            }

            while (diagnostic_index < diagnostics.size() &&
                   diagnostics[diagnostic_index].span.end <= begin) {
                ++diagnostic_index;
            }
            for (std::size_t index = diagnostic_index;
                 index < diagnostics.size(); ++index) {
                const diagnostic &item = diagnostics[index];
                if (item.span.start >= end)
                    break;
                if (item.span.end <= begin || item.span.start >= end)
                    continue;
                const std::size_t under_begin =
                    std::max(begin, item.span.start);
                const std::size_t under_end =
                    std::min(end, item.span.end);
                const int x1 = origin.x + x_at(
                    source,
                    begin,
                    under_begin,
                    editor._tab_width,
                    graphics,
                    values.text_x);
                const int x2 = origin.x + x_at(
                    source,
                    begin,
                    under_end,
                    editor._tab_width,
                    graphics,
                    values.text_x);
                editor.draw_diagnostic(
                    graphics,
                    *painter,
                    item,
                    rect(static_cast<coord>(x1),
                         static_cast<coord>(y),
                         static_cast<dim>(std::max(1, x2 - x1)),
                         static_cast<dim>(values.line_height - 1)),
                    editor_state);
            }
        }

        int caret_x = values.text_x;
        int caret_y = 0;
        if (caret_line >= editor._first_visible_line &&
            caret_line < last_line) {
            const std::size_t begin =
                editor._document->line_start(caret_line);
            caret_x = x_at(source,
                           begin,
                           editor._caret,
                           editor._tab_width,
                           graphics,
                           values.text_x);
            caret_y = (caret_line - editor._first_visible_line) *
                      values.line_height;
            if (editor._focused) {
                theme::state caret_state = editor_state;
                caret_state.selected = colors.current_line.a == 0;
                editor.draw_caret(
                    graphics,
                    *painter,
                    rect(static_cast<coord>(origin.x + caret_x),
                         static_cast<coord>(origin.y + caret_y + 1),
                         1,
                         static_cast<dim>(
                             std::max(1, values.line_height - 2))),
                    caret_state);
            }
        }

        editor.draw_editor_focus(
            graphics, *painter, outer, editor_state);
        draw_completion(editor,
                        graphics,
                        *painter,
                        values,
                        origin,
                        caret_x,
                        caret_y,
                        editor._completion,
                        editor._completion_index,
                        editor._completion_visible);
    }
} // namespace native
