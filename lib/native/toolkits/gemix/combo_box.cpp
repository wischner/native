//
// Implements the GEM themed combo box.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <cctype>
#include <stdexcept>

#include <native/combo_box.h>

#include "globals.h"
#include "../../combo_picker.h"

namespace linux::gemix
{
    native::rect combo_editor_bounds(const native::combo_box &owner) {
        const auto size = owner.get_dimensions();
        const auto *actions = native::detail::peer_state<
            native::detail::combo_picker_actions>(owner);
        const int column =
            actions && actions->step ? int(size.h) / 2 : int(size.h);
        return native::rect(
            1, 0, std::max(1, int(size.w) - column - 1), size.h);
    }

    bool handle_combo_key(native::app_wnd *parent, WORD, WORD key) {
        for (auto *control : combo_boxes) {
            auto *state = combo_box_bindings.object_from_handle(control);
            if (!control || !state || root_of(control) != parent ||
                !state->focused)
                continue;
            const unsigned char character = key & 0xff;
            if (control->get_style() ==
                native::combo_box_style::editable) {
                std::string text = control->get_text();
                if (character == 8 && !text.empty())
                    text.pop_back();
                else if (std::isprint(character))
                    text.push_back(static_cast<char>(character));
                else
                    return false;
                control->on_native_text(text);
                parent->invalidate();
                return true;
            }
            return false;
        }
        return false;
    }
}

namespace native
{
    void combo_box::apply_items() { invalidate(); }
    void combo_box::apply_selected_index() {
        apply_text();
    }
    void combo_box::apply_text() {
        auto *state =
            linux::gemix::combo_box_bindings.object_from_handle(this);
        if (state && state->editor &&
            state->editor->get_text() != get_text()) {
            const bool initial = state->editor->get_text().empty();
            state->editor->set_text(get_text());
            const auto *actions =
                detail::peer_state<detail::combo_picker_actions>(*this);
            if (initial && actions && actions->step &&
                get_text().size() >= 5) {
                // Until the user chooses a component, retain the
                // established default of stepping the minute field.
                auto *editor =
                    linux::gemix::text_edit_bindings.object_from_handle(
                        state->editor.get());
                editor->cursor = editor->anchor = 3;
            }
        }
        invalidate();
    }
    void combo_box::apply_style() { invalidate(); }

    void combo_box::create_native() {
        if (!get_parent() || !get_parent()->get_created())
            throw std::runtime_error(
                "GEMix: combo box requires a created parent.");
        auto *self = this;
        auto *state = new linux::gemix::gem_combo_box;
        linux::gemix::combo_box_bindings.register_pair(self, state);
        linux::gemix::combo_boxes.push_back(self);
        if (auto *actions =
                detail::peer_state<detail::combo_picker_actions>(
                    *this)) {
            actions->square_stepper = bool(actions->step);
            actions->compact_date = bool(actions->open);
            actions->compact_time = bool(actions->step);
            actions->text_margin =
                std::max(1, int(linux::gemix::runtime.char_w) / 2);
            actions->caret = [this] {
                const auto *state =
                    linux::gemix::combo_box_bindings.object_from_handle(
                        this);
                const auto *editor =
                    state && state->editor
                        ? linux::gemix::text_edit_bindings
                              .object_from_handle(state->editor.get())
                        : nullptr;
                return editor ? std::min(editor->cursor, editor->anchor)
                              : std::size_t(3);
            };
        }
    }

    void combo_box::show_native() {
        if (!_created)
            throw std::runtime_error("GEMix: combo box is not created.");
        auto *state =
            linux::gemix::combo_box_bindings.object_from_handle(this);
        if (get_style() == combo_box_style::editable &&
            !state->editor) {
            state->editor = std::make_unique<text_edit>(get_text());
            if (auto *actions = native::detail::peer_state<
                    native::detail::combo_picker_actions>(*this))
                state->editor->set_read_only(actions->read_only);
            state->editor->set_parent(this);
            state->editor->set_border_sides(border_sides::none);
            state->editor->set_bounds(
                linux::gemix::combo_editor_bounds(*this));
            state->editor->create();
            state->editor->show();
            linux::gemix::text_edit_bindings
                .object_from_handle(state->editor.get())
                ->combo_owner = this;
            state->editor->on_change.connect(
                [this](const std::string &value) {
                    on_native_text(value);
                    return true;
                });
        }
        invalidate();
    }

    void combo_box::destroy_native() {
        if (!_created) return;
        auto *self = this;
        auto *state = linux::gemix::combo_box_bindings
                          .object_from_handle(self);
        linux::gemix::combo_boxes.erase(std::remove(
            linux::gemix::combo_boxes.begin(),
            linux::gemix::combo_boxes.end(), self),
            linux::gemix::combo_boxes.end());
        linux::gemix::combo_box_bindings.unregister_by_handle(self);
        delete state;
    }
} // namespace native

namespace native::detail
{
    void configure_picker_day(button &day, bool) {
        day.invalidate();
    }
    point picker_popup_position(combo_box &field) {
        auto *root =
            static_cast<app_wnd *>(linux::gemix::root_of(&field));
        const auto work = linux::gemix::work_rect(
            linux::gemix::wnd_bindings.handle_from_object(root));
        const auto bounds = linux::gemix::root_bounds(field);
        return point(work.x1() + bounds.x1(), work.y1() + bounds.y2());
    }
} // namespace native::detail
