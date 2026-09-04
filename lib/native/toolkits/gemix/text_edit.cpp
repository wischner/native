//
// Implements GEM AES/VDI text editing with selection, validation,
// UTF-8 clipboard commands, and conventional keyboard shortcuts.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/text_edit.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

#include <gem.h>

#include <native/theme.h>

#include "../../text_util.h"
#include "globals.h"
#include "../../control_render_access.h"

namespace
{
    constexpr WORD control_modifier = 0x0004;
    constexpr WORD shift_modifiers = 0x0003;
    constexpr WORD scan_a = 4;
    constexpr WORD scan_c = 6;
    constexpr WORD scan_x = 27;
    constexpr WORD scan_v = 25;
    constexpr WORD scan_home = 74;
    constexpr WORD scan_delete = 76;
    constexpr WORD scan_end = 77;
    constexpr WORD scan_right = 79;
    constexpr WORD scan_left = 80;
    constexpr WORD scan_down = 81;
    constexpr WORD scan_up = 82;

    using binding_type = linux::gemix::gem_text_edit;

    std::pair<std::size_t, std::size_t> selection(
        const binding_type &binding) {
        return std::minmax(binding.cursor, binding.anchor);
    }

    bool replace_selection(native::text_edit *owner,
                           binding_type *binding,
                           const std::string &inserted) {
        if (!owner || !binding || owner->get_read_only())
            return false;
        const auto [begin, end] = selection(*binding);
        std::string candidate = owner->get_text();
        candidate.replace(begin, end - begin, inserted);
        if (!owner->validate(candidate))
            return false;
        binding->cursor = begin + inserted.size();
        binding->anchor = binding->cursor;
        owner->on_native_text(candidate);
        owner->invalidate(owner->get_bounds());
        return true;
    }

    void move_cursor(native::text_edit *owner,
                     binding_type *binding,
                     std::size_t position,
                     bool extend) {
        binding->cursor = std::min(position, owner->get_text().size());
        if (!extend)
            binding->anchor = binding->cursor;
        owner->invalidate(owner->get_bounds());
    }

    std::size_t hit_offset(native::text_edit *owner,
                           int x,
                           int y) {
        const native::rect bounds = owner->get_bounds();
        const int char_width = std::max<int>(
            1, linux::gemix::runtime.char_w);
        const int char_height = std::max<int>(
            1, linux::gemix::runtime.char_h);
        const std::size_t target_line = static_cast<std::size_t>(
            std::max(0, y - bounds.p.y - 3) / char_height);
        const std::string &text = owner->get_text();
        std::size_t begin = 0;
        for (std::size_t line = 0; line < target_line; ++line) {
            const std::size_t newline = text.find('\n', begin);
            if (newline == std::string::npos)
                return text.size();
            begin = newline + 1;
        }
        const std::size_t newline = text.find('\n', begin);
        const std::size_t end = newline == std::string::npos
                                    ? text.size()
                                    : newline;
        const int column = std::max(0, x - bounds.p.x - 3) /
                           char_width;
        std::size_t offset = begin;
        for (int index = 0; index < column && offset < end; ++index) {
            offset = native::detail::next_utf8(text, offset);
        }
        return offset;
    }

    int character_count(const std::string &text,
                        std::size_t begin,
                        std::size_t end) {
        int count = 0;
        while (begin < end) {
            begin = native::detail::next_utf8(text, begin);
            ++count;
        }
        return count;
    }

    std::size_t vertical_offset(const std::string &text,
                                std::size_t cursor,
                                int direction) {
        const std::size_t start = cursor == 0
                                      ? 0
                                      : text.rfind(
                                            '\n', cursor - 1) + 1;
        std::size_t column = static_cast<std::size_t>(
            character_count(text, start, cursor));
        std::size_t target = start;
        std::size_t end = start;
        if (direction < 0) {
            if (start == 0)
                return cursor;
            end = start - 1;
            target = end == 0 ? 0 : text.rfind('\n', end - 1) + 1;
        } else {
            const std::size_t newline = text.find('\n', cursor);
            if (newline == std::string::npos)
                return cursor;
            target = newline + 1;
            end = text.find('\n', target);
            if (end == std::string::npos)
                end = text.size();
        }
        std::size_t result = target;
        while (column > 0 && result < end) {
            result = native::detail::next_utf8(text, result);
            --column;
        }
        return result;
    }

    void draw_editor(native::text_edit *owner,
                     binding_type *binding,
                     native::gpx &g) {
        const native::rect original_clip = g.get_clip();
        const native::rect bounds = owner->get_bounds();
        auto painter = native::theme::create(g);
        const native::theme::palette colors =
            painter->native_palette();
        native::theme::state frame_state;
        frame_state.selected = binding->focused;
        frame_state.disabled = owner->get_read_only();
        native::detail::control_render_access::draw(
            *owner, g, *painter, bounds, frame_state);
        if (bounds.d.w <= 6 || bounds.d.h <= 6)
            return;
        const native::rect content(bounds.p.x + 3,
                                   bounds.p.y + 3,
                                   bounds.d.w - 6,
                                   bounds.d.h - 6);
        g.set_clip(g.get_clip().intersect(content));
        g.set_font(native::font_t::stock(native::font_role::control));
        const int char_width = std::max<int>(
            1, linux::gemix::runtime.char_w);
        const int line_height = std::max<int>(
            1, linux::gemix::runtime.char_h);
        const auto [selected_begin, selected_end] =
            selection(*binding);
        const std::string &text = owner->get_text();
        std::size_t begin = 0;
        int y = content.p.y;
        while (begin <= text.size() && y < content.y2()) {
            const std::size_t newline = text.find('\n', begin);
            const std::size_t end = newline == std::string::npos
                                        ? text.size()
                                        : newline;
            const std::size_t first = std::max(begin, selected_begin);
            const std::size_t last = std::min(end, selected_end);
            if (first < last) {
                const int first_column =
                    character_count(text, begin, first);
                const int selected_columns =
                    character_count(text, first, last);
                g.set_ink(colors.menu_hot_bg)
                    .draw_rect(native::rect(
                                   content.p.x +
                                       first_column * char_width,
                                   y,
                                   std::max(
                                       1,
                                       selected_columns * char_width),
                                   line_height),
                               true);
            }
            const std::string line = text.substr(begin, end - begin);
            g.set_ink(colors.menu_text)
                .draw_text(line, native::point(content.p.x, y));
            if (first < last) {
                const int first_column =
                    character_count(text, begin, first);
                const int selected_columns =
                    character_count(text, first, last);
                const native::rect selected_bounds(
                    content.p.x + first_column * char_width,
                    y,
                    std::max(1, selected_columns * char_width),
                    line_height);
                g.set_clip(original_clip.intersect(content)
                               .intersect(selected_bounds));
                g.set_ink(colors.menu_hot_text)
                    .draw_text(line, native::point(content.p.x, y));
                g.set_clip(original_clip.intersect(content));
            }
            if (binding->focused && binding->cursor >= begin &&
                binding->cursor <= end) {
                const int columns = character_count(
                    text, begin, binding->cursor);
                const int caret_x = content.p.x + columns * char_width;
                g.draw_line(native::point(caret_x, y),
                            native::point(caret_x,
                                          y + line_height - 1));
            }
            if (newline == std::string::npos)
                break;
            begin = newline + 1;
            y += line_height;
        }
    }
} // namespace

namespace linux::gemix
{
    bool focus_text_edit(native::app_wnd *parent,
                         native::point point) {
        native::text_edit *hit = nullptr;
        for (native::text_edit *editor : text_edits) {
            auto *binding =
                text_edit_bindings.object_from_handle(editor);
            if (binding && editor->get_parent() == parent)
                binding->focused = false;
            if (binding && editor->get_parent() == parent &&
                binding->visible &&
                editor->get_bounds().contains(point)) {
                hit = editor;
            }
        }
        if (!hit) {
            for (native::text_edit *editor : text_edits) {
                auto *binding =
                    text_edit_bindings.object_from_handle(editor);
                if (binding && editor->get_parent() == parent)
                    parent->invalidate(editor->get_bounds());
            }
            return false;
        }
        auto *binding = text_edit_bindings.object_from_handle(hit);
        binding->focused = true;
        binding->cursor = hit_offset(hit, point.x, point.y);
        binding->anchor = binding->cursor;
        parent->invalidate(hit->get_bounds());
        return true;
    }

    bool handle_text_edit_key(native::app_wnd *parent,
                              WORD modifiers,
                              WORD key) {
        native::text_edit *owner = nullptr;
        gem_text_edit *binding = nullptr;
        for (native::text_edit *editor : text_edits) {
            auto *candidate =
                text_edit_bindings.object_from_handle(editor);
            if (candidate && editor->get_parent() == parent &&
                candidate->visible && candidate->focused) {
                owner = editor;
                binding = candidate;
                break;
            }
        }
        if (!owner)
            return false;
        const WORD scan = static_cast<WORD>((key >> 8) & 0xff);
        const char character = static_cast<char>(key & 0xff);
        if ((modifiers & control_modifier) != 0) {
            switch (scan) {
            case scan_a:
                owner->select_all();
                return true;
            case scan_c:
                owner->copy();
                return true;
            case scan_x:
                owner->cut();
                return true;
            case scan_v:
                owner->paste();
                return true;
            default:
                return false;
            }
        }
        const bool extend = (modifiers & shift_modifiers) != 0;
        const auto [begin, end] = selection(*binding);
        if (character == 8) {
            if (begin == end && binding->cursor != 0) {
                binding->anchor = native::detail::previous_utf8(
                    owner->get_text(), binding->cursor);
            }
            return replace_selection(owner, binding, "");
        }
        if (scan == scan_delete) {
            if (begin == end &&
                binding->cursor < owner->get_text().size()) {
                binding->anchor = native::detail::next_utf8(
                    owner->get_text(), binding->cursor);
            }
            return replace_selection(owner, binding, "");
        }
        if (scan == scan_left) {
            move_cursor(owner,
                        binding,
                        native::detail::previous_utf8(
                            owner->get_text(), binding->cursor),
                        extend);
            return true;
        }
        if (scan == scan_right) {
            move_cursor(owner,
                        binding,
                        native::detail::next_utf8(
                            owner->get_text(), binding->cursor),
                        extend);
            return true;
        }
        if (scan == scan_up || scan == scan_down) {
            move_cursor(owner,
                        binding,
                        vertical_offset(
                            owner->get_text(),
                            binding->cursor,
                            scan == scan_up ? -1 : 1),
                        extend);
            return true;
        }
        if (scan == scan_home) {
            const std::size_t start =
                binding->cursor == 0
                    ? 0
                    : owner->get_text().rfind(
                          '\n', binding->cursor - 1) + 1;
            move_cursor(owner, binding, start, extend);
            return true;
        }
        if (scan == scan_end) {
            const std::size_t newline =
                owner->get_text().find('\n', binding->cursor);
            move_cursor(owner,
                        binding,
                        newline == std::string::npos
                            ? owner->get_text().size()
                            : newline,
                        extend);
            return true;
        }
        if (character == '\r' || character == '\n') {
            return owner->get_mode() ==
                           native::text_edit_mode::multi_line
                       ? replace_selection(owner, binding, "\n")
                       : true;
        }
        if (static_cast<unsigned char>(character) >= 32)
            return replace_selection(
                owner, binding, std::string(1, character));
        return false;
    }

    void render_text_edits(native::app_wnd *parent, native::gpx &g) {
        const native::rect clip = g.get_clip();
        for (native::text_edit *editor : text_edits) {
            auto *binding =
                text_edit_bindings.object_from_handle(editor);
            if (binding && editor->get_parent() == parent &&
                binding->visible) {
                g.set_clip(clip);
                draw_editor(editor, binding, g);
            }
        }
        g.set_clip(clip);
    }
} // namespace linux::gemix

namespace native
{
    void text_edit::apply_text() {
        auto *binding =
            linux::gemix::text_edit_bindings.object_from_handle(this);
        if (!binding)
            throw std::runtime_error(
                "GEMix: Missing text-edit binding.");
        binding->cursor = std::min(binding->cursor, _text.size());
        binding->anchor = std::min(binding->anchor, _text.size());
        invalidate();
    }

    void text_edit::apply_read_only() {
        invalidate();
    }

    void text_edit::create_native() {
        auto *parent = dynamic_cast<app_wnd *>(get_parent());
        if (!parent || !parent->get_created())
            throw std::runtime_error(
                "GEMix: text_edit requires a created app_wnd parent.");
        auto *self = this;
        auto *binding = new linux::gemix::gem_text_edit;
        binding->cursor = _text.size();
        binding->anchor = binding->cursor;
        linux::gemix::text_edit_bindings.register_pair(self, binding);
        linux::gemix::text_edits.push_back(self);
    }

    void text_edit::show_native() {
        auto *binding =
            linux::gemix::text_edit_bindings.object_from_handle(
                this);
        if (!_created || !binding)
            throw std::runtime_error(
                "GEMix: text_edit is not created.");
        binding->visible = true;
        invalidate();
    }

    void text_edit::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *binding =
            linux::gemix::text_edit_bindings.object_from_handle(self);
        linux::gemix::text_edits.erase(
            std::remove(linux::gemix::text_edits.begin(),
                        linux::gemix::text_edits.end(),
                        self),
            linux::gemix::text_edits.end());
        linux::gemix::text_edit_bindings.unregister_by_handle(self);
        delete binding;
    }

    std::string text_edit::selected_text() const {
        auto *binding =
            linux::gemix::text_edit_bindings.object_from_handle(
                const_cast<text_edit *>(this));
        if (!binding)
            return {};
        const auto [begin, end] = selection(*binding);
        return _text.substr(begin, end - begin);
    }

    bool text_edit::replace_selected_text(const std::string &text) {
        auto *binding =
            linux::gemix::text_edit_bindings.object_from_handle(
                const_cast<text_edit *>(this));
        return replace_selection(this, binding, text);
    }

    void text_edit::select_all_native() {
        auto *binding =
            linux::gemix::text_edit_bindings.object_from_handle(
                this);
        if (binding) {
            binding->anchor = 0;
            binding->cursor = _text.size();
            invalidate();
        }
    }
} // namespace native
