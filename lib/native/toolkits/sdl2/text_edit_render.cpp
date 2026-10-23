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

#include "../../control_render_access.h"
#include "globals.h"

namespace
{
    void draw_editor(native::text_edit *owner,
                     linux::sdl2::sdl2_text_edit *binding,
                     native::gpx &g) {
        const native::rect bounds = binding->bounds;
        const bool focused = native::detail::input_target(
                                 *linux::sdl2::root_of(owner)) == owner;
        auto painter = native::theme::create(g);
        const native::theme::palette colors =
            painter->native_palette();
        native::theme::state frame_state;
        frame_state.selected = focused;
        frame_state.disabled = !owner->get_input_enabled();
        const bool combo_editor =
            dynamic_cast<native::combo_box *>(owner->get_parent());
        if (combo_editor)
            g.set_ink(colors.content_bg)
                .draw_rect(
                    native::rect(bounds.x1() + 1,
                                 bounds.y1() + 1,
                                 std::max(0, int(bounds.w()) - 2),
                                 std::max(0, int(bounds.h()) - 2)),
                    true);
        else
            native::detail::control_render_access::draw(
                *owner, g, *painter, bounds, frame_state);
        const int padding =
            combo_editor || owner->get_border_sides() !=
                                native::border_sides::none
                ? 4
                : 1;
        if (bounds.d.w <= 2 * padding || bounds.d.h <= 2 * padding)
            return;

        const int search = dynamic_cast<native::settings_sidebar *>(
                               owner->get_parent())
                               ? 18
                               : 0;
        if (search) {
            const int center = bounds.y1() + int(bounds.h()) / 2;
            g.set_pen(1)
                .set_ink(colors.content_text)
                .draw_ellipse(
                    native::rect(bounds.x1() + 6, center - 5, 8, 8),
                    false)
                .draw_line(native::point(bounds.x1() + 13, center + 2),
                           native::point(bounds.x1() + 17, center + 6));
        }
        const native::rect content(
            bounds.p.x + padding + search,
            bounds.p.y + padding,
            std::max(0, int(bounds.d.w) - 2 * padding - search),
            bounds.d.h - 2 * padding);
        g.set_clip(g.get_clip().intersect(content));
        g.set_font(native::font_t::stock(native::font_role::control));
        const int line_height =
            std::max(1, linux::sdl2::text_height() + 2);
        const auto [selected_begin, selected_end] =
            std::minmax(binding->cursor, binding->anchor);
        const std::string &text = owner->get_text();
        std::size_t begin = 0;
        std::size_t line = 0;
        int y = owner->get_mode() == native::text_edit_mode::single_line
            ? bounds.y1() + std::max(0, (int(bounds.h()) - linux::sdl2::text_height()) / 2)
            : content.p.y;
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
                    const native::rgba selection_background =
                        focused ? colors.selection_bg
                                : colors.selection_inactive_bg;
                    const native::rgba selection_text =
                        focused ? colors.selection_text
                                : colors.selection_inactive_text;
                    g.set_ink(selection_background)
                        .draw_rect(native::rect(
                                       x1,
                                       y,
                                       static_cast<native::dim>(
                                           std::max(1, x2 - x1)),
                                       static_cast<native::dim>(
                                           line_height)),
                                   true);
                    g.set_ink(colors.button_text)
                        .draw_text(
                            text.substr(begin, first - begin),
                            native::point(origin_x, y))
                        .set_ink(selection_text)
                        .draw_text(
                            text.substr(first, last - first),
                            native::point(x1, y))
                        .set_ink(colors.button_text)
                        .draw_text(
                            text.substr(last, end - last),
                            native::point(x2, y));
                } else {
                    g.set_ink(colors.button_text)
                        .draw_text(text.substr(begin, end - begin),
                                   native::point(origin_x, y));
                }
                if (focused && binding->cursor >= begin &&
                    binding->cursor <= end) {
                    const int caret_x = origin_x +
                                        linux::sdl2::text_width(
                                            text.substr(
                                                begin,
                                                binding->cursor -
                                                    begin));
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
            auto *binding =
                text_edit_bindings.object_from_handle(editor);
            if (binding && root_of(editor) == parent && binding->visible) {
                binding->bounds = root_bounds(*editor);
                g.set_clip(old_clip);
                draw_editor(editor, binding, g);
            }
        }
        g.set_clip(old_clip);
    }
} // namespace linux::sdl2
