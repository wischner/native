//
// Implements SDL-emulated text editing, selection, live validation,
// UTF-8 navigation, and portable clipboard shortcuts.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native/text_edit.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

#include <SDL2/SDL.h>

#include "../../text_util.h"
#include "globals.h"

namespace
{
    using binding_type = linux::sdl2::sdl2_text_edit;

    std::pair<std::size_t, std::size_t> selection(
        const binding_type &binding) {
        return std::minmax(binding.cursor, binding.anchor);
    }

    void invalidate_binding(binding_type *binding) {
        if (binding && binding->parent)
            binding->parent->invalidate();
    }

    void update_scroll(native::text_edit *owner,
                       binding_type *binding) {
        if (!owner || !binding)
            return;
        const std::string &text = owner->get_text();
        const std::size_t line_start =
            binding->cursor == 0
                ? 0
                : text.rfind('\n', binding->cursor - 1) + 1;
        std::size_t line = 0;
        for (std::size_t i = 0; i < line_start; ++i) {
            if (text[i] == '\n')
                ++line;
        }
        const int line_height =
            std::max(1, linux::sdl2::text_height() + 2);
        const int content_height =
            std::max(1, static_cast<int>(binding->bounds.d.h) - 8);
        const std::size_t visible = static_cast<std::size_t>(
            std::max(1, content_height / line_height));
        if (line < binding->first_line)
            binding->first_line = line;
        if (line >= binding->first_line + visible)
            binding->first_line = line - visible + 1;

        const int cursor_x = linux::sdl2::text_width(
            text.substr(line_start, binding->cursor - line_start));
        const int width = std::max(1, binding->bounds.d.w - 10);
        if (cursor_x < binding->horizontal_scroll)
            binding->horizontal_scroll = cursor_x;
        if (cursor_x >= binding->horizontal_scroll + width)
            binding->horizontal_scroll = cursor_x - width + 1;
        if (owner->get_mode() == native::text_edit_mode::multi_line)
            binding->horizontal_scroll = 0;
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
        update_scroll(owner, binding);
        invalidate_binding(binding);
        return true;
    }

    std::size_t hit_offset(native::text_edit *owner,
                           binding_type *binding,
                           int x,
                           int y) {
        const std::string &text = owner->get_text();
        const int line_height =
            std::max(1, linux::sdl2::text_height() + 2);
        std::size_t target_line = binding->first_line;
        if (y > binding->bounds.p.y + 4) {
            target_line += static_cast<std::size_t>(
                (y - binding->bounds.p.y - 4) / line_height);
        }
        std::size_t begin = 0;
        for (std::size_t line = 0; line < target_line; ++line) {
            const std::size_t newline = text.find('\n', begin);
            if (newline == std::string::npos)
                return text.size();
            begin = newline + 1;
        }
        const std::size_t end = text.find('\n', begin);
        const std::size_t limit =
            end == std::string::npos ? text.size() : end;
        const int local_x = x - binding->bounds.p.x - 4 +
                            binding->horizontal_scroll;
        std::size_t offset = begin;
        while (offset < limit) {
            const std::size_t next =
                native::detail::next_utf8(text, offset);
            const int right = linux::sdl2::text_width(
                text.substr(begin, next - begin));
            const int left = linux::sdl2::text_width(
                text.substr(begin, offset - begin));
            if (local_x < (left + right) / 2)
                break;
            offset = next;
        }
        return offset;
    }

    void move_cursor(native::text_edit *owner,
                     binding_type *binding,
                     std::size_t position,
                     bool extend) {
        binding->cursor = std::min(position, owner->get_text().size());
        if (!extend)
            binding->anchor = binding->cursor;
        update_scroll(owner, binding);
        invalidate_binding(binding);
    }

    std::size_t vertical_offset(const std::string &text,
                                std::size_t cursor,
                                int direction) {
        const std::size_t start = cursor == 0
                                      ? 0
                                      : text.rfind(
                                            '\n', cursor - 1) + 1;
        std::size_t column = 0;
        for (std::size_t offset = start; offset < cursor; ++column)
            offset = native::detail::next_utf8(text, offset);
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

} // namespace

namespace linux::sdl2
{
    bool handle_text_edit_mouse(native::wnd *parent,
                                int x,
                                int y,
                                bool pressed) {
        if (!pressed) {
            for (native::text_edit *editor : text_edits) {
                auto *binding =
                    text_edit_bindings.object_from_handle(editor);
                if (binding && root_of(editor) == parent &&
                    binding->mouse_selecting) {
                    binding->cursor = hit_offset(
                        editor, binding, x, y);
                    binding->mouse_selecting = false;
                    invalidate_binding(binding);
                    return true;
                }
            }
            return false;
        }
        native::text_edit *hit = nullptr;
        for (native::text_edit *editor : text_edits) {
            auto *binding =
                text_edit_bindings.object_from_handle(editor);
            if (binding && root_of(editor) == parent &&
                binding->visible &&
                binding->bounds.contains(native::point(x, y))) {
                hit = editor;
            }
            if (binding && root_of(editor) == parent)
                binding->focused = false;
            if (binding && root_of(editor) == parent)
                binding->mouse_selecting = false;
        }
        if (!hit) {
            SDL_StopTextInput();
            parent->invalidate();
            return false;
        }
        auto *binding = text_edit_bindings.object_from_handle(hit);
        binding->focused = true;
        binding->cursor = hit_offset(hit, binding, x, y);
        if ((SDL_GetModState() & KMOD_SHIFT) == 0)
            binding->anchor = binding->cursor;
        binding->mouse_selecting = true;
        SDL_StartTextInput();
        parent->invalidate();
        return true;
    }

    bool handle_text_edit_motion(native::wnd *parent, int x, int y) {
        for (native::text_edit *editor : text_edits) {
            auto *binding =
                text_edit_bindings.object_from_handle(editor);
            if (binding && root_of(editor) == parent &&
                binding->mouse_selecting) {
                binding->cursor = hit_offset(editor, binding, x, y);
                update_scroll(editor, binding);
                invalidate_binding(binding);
                return true;
            }
        }
        return false;
    }

    bool handle_text_edit_key(native::wnd *parent,
                              const SDL_KeyboardEvent &event) {
        if (event.type != SDL_KEYDOWN)
            return false;
        native::text_edit *owner = nullptr;
        sdl2_text_edit *binding = nullptr;
        for (native::text_edit *editor : text_edits) {
            auto *candidate =
                text_edit_bindings.object_from_handle(editor);
            if (candidate && root_of(editor) == parent &&
                candidate->visible && candidate->focused) {
                owner = editor;
                binding = candidate;
                break;
            }
        }
        if (!owner)
            return false;
        const bool control = (event.keysym.mod & KMOD_CTRL) != 0;
        if (control) {
            switch (event.keysym.sym) {
            case SDLK_a:
                owner->select_all();
                break;
            case SDLK_c:
                owner->copy();
                break;
            case SDLK_x:
                owner->cut();
                break;
            case SDLK_v:
                owner->paste();
                break;
            default:
                return false;
            }
            return true;
        }

        const bool extend = (event.keysym.mod & KMOD_SHIFT) != 0;
        const auto [begin, end] = selection(*binding);
        switch (event.keysym.sym) {
        case SDLK_BACKSPACE:
            if (begin != end)
                return replace_selection(owner, binding, "");
            if (binding->cursor != 0) {
                binding->anchor = native::detail::previous_utf8(
                    owner->get_text(), binding->cursor);
                return replace_selection(owner, binding, "");
            }
            return true;
        case SDLK_DELETE:
            if (begin != end)
                return replace_selection(owner, binding, "");
            if (binding->cursor < owner->get_text().size()) {
                binding->anchor = native::detail::next_utf8(
                    owner->get_text(), binding->cursor);
                return replace_selection(owner, binding, "");
            }
            return true;
        case SDLK_LEFT:
            move_cursor(owner,
                        binding,
                        native::detail::previous_utf8(
                            owner->get_text(), binding->cursor),
                        extend);
            return true;
        case SDLK_RIGHT:
            move_cursor(owner,
                        binding,
                        native::detail::next_utf8(
                            owner->get_text(), binding->cursor),
                        extend);
            return true;
        case SDLK_UP:
        case SDLK_DOWN:
            move_cursor(owner,
                        binding,
                        vertical_offset(
                            owner->get_text(),
                            binding->cursor,
                            event.keysym.sym == SDLK_UP ? -1 : 1),
                        extend);
            return true;
        case SDLK_HOME: {
            const std::size_t start =
                binding->cursor == 0
                    ? 0
                    : owner->get_text().rfind(
                          '\n', binding->cursor - 1) + 1;
            move_cursor(owner, binding, start, extend);
            return true;
        }
        case SDLK_END: {
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
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (owner->get_mode() ==
                native::text_edit_mode::multi_line) {
                return replace_selection(owner, binding, "\n");
            }
            return true;
        default:
            return false;
        }
    }

    bool handle_text_edit_input(native::wnd *parent,
                                const char *text) {
        for (native::text_edit *editor : text_edits) {
            auto *binding =
                text_edit_bindings.object_from_handle(editor);
            if (binding && root_of(editor) == parent &&
                binding->visible && binding->focused) {
                return replace_selection(
                    editor, binding, text ? text : "");
            }
        }
        return false;
    }

} // namespace linux::sdl2

namespace native
{
    void text_edit::apply_text() {
        auto *binding =
            linux::sdl2::text_edit_bindings.object_from_handle(this);
        if (!binding)
            throw std::runtime_error(
                "SDL2: Missing text-edit binding.");
        binding->cursor = std::min(binding->cursor, _text.size());
        binding->anchor = std::min(binding->anchor, _text.size());
        update_scroll(this, binding);
        invalidate_binding(binding);
    }

    void text_edit::apply_read_only() {
        auto *binding =
            linux::sdl2::text_edit_bindings.object_from_handle(this);
        if (!binding)
            throw std::runtime_error(
                "SDL2: Missing text-edit binding.");
        invalidate_binding(binding);
    }

    void text_edit::create() const {
        if (_created)
            return;
        wnd *parent = get_parent();
        if (!parent || !parent->get_created())
            throw std::runtime_error(
                "SDL2: text_edit requires a created parent.");
        auto *self = const_cast<text_edit *>(this);
        auto *binding = new linux::sdl2::sdl2_text_edit;
        binding->parent = parent;
        binding->bounds = linux::sdl2::root_bounds(*this);
        binding->cursor = _text.size();
        binding->anchor = binding->cursor;
        linux::sdl2::text_edit_bindings.register_pair(self, binding);
        linux::sdl2::text_edits.push_back(self);
        _created = true;
        self->on_native_create();
    }

    void text_edit::show() const {
        auto *binding =
            linux::sdl2::text_edit_bindings.object_from_handle(
                const_cast<text_edit *>(this));
        if (!_created || !binding)
            throw std::runtime_error(
                "SDL2: text_edit is not created.");
        binding->visible = true;
        invalidate_binding(binding);
    }

    void text_edit::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<text_edit *>(this);
        auto *binding =
            linux::sdl2::text_edit_bindings.object_from_handle(self);
        self->on_native_destroy();
        auto &editors = linux::sdl2::text_edits;
        editors.erase(std::remove(editors.begin(), editors.end(), self),
                      editors.end());
        invalidate_binding(binding);
        linux::sdl2::text_edit_bindings.unregister_by_handle(self);
        delete binding;
    }

    std::string text_edit::selected_text() const {
        auto *binding =
            linux::sdl2::text_edit_bindings.object_from_handle(
                const_cast<text_edit *>(this));
        if (!binding)
            return {};
        const auto [begin, end] = selection(*binding);
        return _text.substr(begin, end - begin);
    }

    bool text_edit::replace_selected_text(const std::string &text) {
        auto *binding =
            linux::sdl2::text_edit_bindings.object_from_handle(this);
        return replace_selection(this, binding, text);
    }

    void text_edit::select_all_native() const {
        auto *binding =
            linux::sdl2::text_edit_bindings.object_from_handle(
                const_cast<text_edit *>(this));
        if (binding) {
            binding->anchor = 0;
            binding->cursor = _text.size();
            invalidate_binding(binding);
        }
    }
} // namespace native
