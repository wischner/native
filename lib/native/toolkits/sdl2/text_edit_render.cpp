//
// Renders SDL-emulated text-editor frames, selections, text, and carets
// through the SDL backend theme and graphics context.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <string>
#include <utility>

#include <native.h>
#include <native/text_edit.h>
#include <native/theme.h>

#include "globals.h"

namespace
{
    void draw_editor(native::text_edit *owner,
                     linux::sdl2::sdl2_text_edit *binding,
                     native::gpx &g) {
        const native::rect bounds = binding->bounds;
        auto painter = native::theme::create(g);
        const native::theme::palette colors =
            painter->native_palette();
        native::theme::state frame_state;
        frame_state.selected = binding->focused;
        frame_state.disabled = owner->get_read_only();
        painter->draw_text_edit_frame(bounds, frame_state);
        if (bounds.d.w <= 8 || bounds.d.h <= 8)
            return;

        const native::rect content(bounds.p.x + 4,
                                   bounds.p.y + 4,
                                   bounds.d.w - 8,
                                   bounds.d.h - 8);
        g.set_clip(g.get_clip().intersect(content));
        g.set_font(native::font_t::stock(native::font_role::control));
        const int line_height =
            std::max(1, linux::sdl2::text_height() + 2);
        const auto [selected_begin, selected_end] =
            std::minmax(binding->cursor, binding->anchor);
        const std::string &text = owner->get_text();
        std::size_t begin = 0;
        std::size_t line = 0;
        int y = content.p.y;
        while (begin <= text.size() && y < content.y2()) {
            const std::size_t newline = text.find('\n', begin);
            const std::size_t end = newline == std::string::npos
                                        ? text.size()
                                        : newline;
            if (line >= binding->first_line) {
                const int origin_x = content.p.x -
                                     binding->horizontal_scroll;
                const std::size_t first =
                    std::max(begin, selected_begin);
                const std::size_t last = std::min(end, selected_end);
                if (first < last) {
                    const int x1 = origin_x + linux::sdl2::text_width(
                        text.substr(begin, first - begin));
                    const int x2 = origin_x + linux::sdl2::text_width(
                        text.substr(begin, last - begin));
                    g.set_ink(native::rgba(51, 103, 209, 255))
                        .draw_rect(native::rect(
                                       x1,
                                       y,
                                       static_cast<native::dim>(
                                           std::max(1, x2 - x1)),
                                       static_cast<native::dim>(
                                           line_height)),
                                   true);
                }
                g.set_ink(colors.button_text)
                    .draw_text(text.substr(begin, end - begin),
                               native::point(origin_x, y));
                if (binding->focused && binding->cursor >= begin &&
                    binding->cursor <= end) {
                    const int caret_x = origin_x +
                                        linux::sdl2::text_width(
                                            text.substr(
                                                begin,
                                                binding->cursor - begin));
                    g.set_ink(colors.button_text)
                        .draw_line(native::point(caret_x, y),
                                   native::point(
                                       caret_x,
                                       y + linux::sdl2::text_height()));
                }
                y += line_height;
            }
            if (newline == std::string::npos)
                break;
            begin = newline + 1;
            ++line;
        }
    }
} // namespace

namespace linux::sdl2
{
    void render_text_edits(native::wnd *parent, native::gpx &g) {
        const native::rect old_clip = g.get_clip();
        for (native::text_edit *editor : text_edits) {
            auto *binding = text_edit_bindings.object_from_handle(editor);
            if (binding && binding->parent == parent && binding->visible) {
                g.set_clip(old_clip);
                draw_editor(editor, binding, g);
            }
        }
        g.set_clip(old_clip);
    }
} // namespace linux::sdl2
