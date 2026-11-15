//
// Implements the GEM themed combo box.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

#include <native/combo_box.h>

#include "globals.h"
#include "../../input_state.h"
#include "../../combo_picker.h"

namespace linux::gemix
{
    // Native selection notifications normally come from a widget that has
    // already changed its text. GEM must update its private editor first.
    static void select_item(native::combo_box &control, int index) {
        auto *state = combo_box_bindings.object_from_handle(&control);
        if (state && state->editor) {
            state->editor->set_text(control.get_items().at(index));
            auto *entry = text_edit_bindings.object_from_handle(state->editor.get());
            entry->cursor = entry->anchor = state->editor->get_text().size();
        }
        control.on_native_selection(index);
    }

    native::rect combo_editor_bounds(const native::combo_box &owner) {
        const auto size = owner.get_dimensions();
        const auto *actions = native::detail::peer_state<
            native::detail::combo_picker_actions>(owner);
        const int column =
            actions && actions->step ? int(size.h) / 2 : int(size.h);
        return native::rect(
            1, 0, std::max(1, int(size.w) - column - 1), size.h);
    }

    bool dispatch_combo_popup_click(native::app_wnd *owner,
                                    native::point position,
                                    bool pressed) {
        for (auto *control : combo_boxes) {
            auto *state = control
                ? combo_box_bindings.object_from_handle(control)
                : nullptr;
            if (!state || !state->open || root_of(control) != owner ||
                !visible_in_tree(*control) || !control->get_input_enabled())
                continue;
            // The overlay owns the entire gesture, including an outside
            // dismissal. Do not let a canvas below it take press capture.
            if (pressed) return true;
            const auto bounds = root_bounds(*control);
            const int row = native::theme::create(owner->get_gpx())
                                ->get_list_item_height();
            const native::rect popup(
                bounds.x1() + 1, bounds.y2() + 1,
                std::max(0, int(bounds.w()) - 2),
                control->get_items().size() * row);
            const int selected = popup.contains(position)
                ? (position.y - popup.y1()) / row : -1;
            const native::detail::window_guard owner_guard(*owner);
            const native::detail::window_guard control_guard(*control);
            state->open = false;
            state->focused = true;
            if (selected >= 0) select_item(*control, selected);
            if (control_guard.alive()) control->on_native_drop_down(false);
            if (owner_guard.alive()) owner->invalidate();
            return true;
        }
        return false;
    }

    void dismiss_combo_popups(native::app_wnd &owner) {
        const native::detail::window_guard owner_guard(owner);
        // Drop-down callbacks may remove controls from the live registry.
        using guarded_combo = std::pair<native::combo_box *,
                                        native::detail::window_guard>;
        std::vector<guarded_combo> controls;
        for (auto *control : combo_boxes)
            controls.emplace_back(control, native::detail::window_guard(*control));
        for (const auto &[control, guard] : controls) {
            if (!owner_guard.alive()) return;
            if (!guard.alive()) continue;
            auto *state = combo_box_bindings.object_from_handle(control);
            if (!state || !state->open || root_of(control) != &owner) continue;
            state->open = false;
            owner.invalidate();
            control->on_native_drop_down(false);
        }
    }

    bool handle_combo_key(native::app_wnd *parent, WORD modifiers, WORD key) {
        const auto *focus = native::detail::input_target(*parent);
        for (auto *control : combo_boxes) {
            auto *state = combo_box_bindings.object_from_handle(control);
            if (!control || !state || root_of(control) != parent ||
                (focus != control && focus != state->editor.get()))
                continue;
            const unsigned scan = (unsigned(key) >> 8) & 255;
            const unsigned character = key & 255;
            const bool up = scan == 82, down = scan == 81;
            const bool enter = character == 13 || character == 10;
            const bool toggle = scan == 61 || ((modifiers & 8) && down);
            if (modifiers & 4) return false;
            if ((modifiers & 8) && !toggle) return false;
            if (auto *actions = native::detail::peer_state<
                    native::detail::combo_picker_actions>(*control)) {
                std::function<void()> action;
                if (toggle || (enter && actions->read_only)) action = actions->open;
                else if (enter) action = actions->commit;
                else if (up || down) {
                    const auto step = actions->keyboard_step ? actions->keyboard_step : actions->step;
                    if (step) action = [step, up] { step(up ? 1 : -1); };
                }
                if (!action) return false;
                action();
                return true;
            }
            const native::detail::window_guard control_guard(*control);
            if (state->open) {
                if (character == 27 || enter || toggle) {
                    const int selected = state->hot;
                    state->open = false;
                    parent->invalidate();
                    if (enter && selected >= 0 &&
                        selected < static_cast<int>(control->get_items().size()))
                        select_item(*control, selected);
                    if (control_guard.alive()) control->on_native_drop_down(false);
                    return true;
                }
                if (up || down) {
                    const int count = static_cast<int>(control->get_items().size());
                    if (count > 0)
                        state->hot = std::clamp(state->hot + (down ? 1 : -1),
                                                0, count - 1);
                    parent->invalidate();
                    return true;
                }
                // An open popup owns text; do not edit behind it.
                return true;
            }
            if (toggle || up || down ||
                (control->get_style() == native::combo_box_style::drop_down_list &&
                 (enter || character == 32))) {
                if (control->get_items().empty()) return true;
                state->hot = std::max(0, control->get_selected_index());
                state->open = true;
                parent->invalidate();
                control->on_native_drop_down(true);
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
            actions->plain_calendar = true;
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
            // This private field retains the combo's native Enter command.
            state->editor->set_enter_as_tab(false);
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
