//
// Implements the SDL themed combo box.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>

#include <native/combo_box.h>

#include "../../control_render_access.h"
#include "globals.h"

namespace linux::sdl2
{
    bool handle_combo_mouse(native::wnd *owner,
                            int x, int y, bool released) {
        if (!released) return false;
        bool consumed = false;
        for (auto *combo : combo_boxes) {
            auto *state = combo_box_bindings.object_from_handle(combo);
            if (!state || state->parent != owner || !state->visible) continue;
            const native::rect bounds = combo->get_bounds();
            const int item_height = 20;
            const native::rect popup(
                bounds.x1(), bounds.y2(), bounds.w(),
                static_cast<native::dim>(
                    combo->get_items().size()*item_height));
            if (state->open && popup.contains({
                    static_cast<native::coord>(x),
                    static_cast<native::coord>(y)})) {
                const int index = (y-popup.y1())/item_height;
                combo->on_native_selection(index);
                state->open = false;
                combo->on_native_drop_down(false);
                consumed = true;
            } else if (bounds.contains({static_cast<native::coord>(x),
                                        static_cast<native::coord>(y)})) {
                state->focused = true;
                state->open = !state->open;
                combo->on_native_drop_down(state->open);
                consumed = true;
            } else {
                if (state->open) combo->on_native_drop_down(false);
                state->open = false;
                state->focused = false;
            }
        }
        if (consumed) owner->invalidate();
        return consumed;
    }

    bool handle_combo_key(native::wnd *owner,
                          const SDL_KeyboardEvent &event) {
        if (event.type != SDL_KEYDOWN) return false;
        for (auto *combo : combo_boxes) {
            auto *state = combo_box_bindings.object_from_handle(combo);
            if (!state || state->parent != owner || !state->focused ||
                !state->visible) continue;
            if (event.keysym.sym == SDLK_BACKSPACE &&
                combo->get_style() == native::combo_box_style::editable &&
                !combo->get_text().empty()) {
                std::string text = combo->get_text();
                text.pop_back();
                combo->on_native_text(text);
                owner->invalidate();
            } else if (event.keysym.sym == SDLK_ESCAPE && state->open) {
                state->open = false;
                combo->on_native_drop_down(false);
                owner->invalidate();
            } else if (event.keysym.sym == SDLK_DOWN ||
                       event.keysym.sym == SDLK_UP) {
                if (combo->get_items().empty())
                    return true;
                int index = combo->get_selected_index();
                index += event.keysym.sym == SDLK_DOWN ? 1 : -1;
                index = std::clamp(index, 0,
                    static_cast<int>(combo->get_items().size())-1);
                combo->on_native_selection(index);
                owner->invalidate();
            }
            return true;
        }
        return false;
    }

    bool handle_combo_text(native::wnd *owner, const char *value) {
        for (auto *combo : combo_boxes) {
            auto *state = combo_box_bindings.object_from_handle(combo);
            if (!state || state->parent != owner || !state->focused ||
                !state->visible ||
                combo->get_style() != native::combo_box_style::editable)
                continue;
            combo->on_native_text(combo->get_text()+
                                  (value ? value : ""));
            owner->invalidate();
            return true;
        }
        return false;
    }

    void render_combo_boxes(native::wnd *owner, native::gpx &graphics) {
        auto appearance = native::theme::create(graphics);
        const auto metrics = appearance->defaults();
        for (auto *combo : combo_boxes) {
            auto *state = combo_box_bindings.object_from_handle(combo);
            if (!state || state->parent != owner || !state->visible) continue;
            native::theme::state control_state;
            control_state.focused = state->focused;
            native::detail::control_render_access::draw(
                *combo, graphics, *appearance,
                combo->get_bounds(), control_state);
            if (!state->open) continue;
            int y = combo->get_bounds().y2();
            const int row_height = metrics.list_item_height;
            const native::rect popup(combo->get_bounds().x1(),
                static_cast<native::coord>(y), combo->get_bounds().w(),
                static_cast<native::dim>(
                    combo->get_items().size()*row_height));
            appearance->draw_popup_frame(popup);
            for (std::size_t index = 0;
                 index < combo->get_items().size(); ++index) {
                native::theme::state item_state;
                item_state.selected =
                    static_cast<int>(index) == combo->get_selected_index();
                appearance->draw_list_item(
                    native::rect(popup.x1(),
                        static_cast<native::coord>(y+index*row_height),
                        popup.w(), static_cast<native::dim>(row_height)),
                    combo->get_items()[index], item_state);
            }
        }
    }
} // namespace linux::sdl2

namespace native
{
    void combo_box::apply_items() { invalidate(); }
    void combo_box::apply_selected_index() { invalidate(); }
    void combo_box::apply_text() { invalidate(); }
    void combo_box::apply_style() { invalidate(); }

    void combo_box::create() const {
        if (_created) return;
        wnd *parent = get_parent();
        if (!parent || !parent->get_created())
            throw std::runtime_error(
                "SDL2: combo box requires a created parent.");
        auto *self = const_cast<combo_box *>(this);
        auto *state = new linux::sdl2::sdl2_combo_box;
        state->parent = parent;
        linux::sdl2::combo_box_bindings.register_pair(self, state);
        linux::sdl2::combo_boxes.push_back(self);
        _created = true;
        self->on_native_create();
    }

    void combo_box::show() const {
        auto *state = linux::sdl2::combo_box_bindings.object_from_handle(
            const_cast<combo_box *>(this));
        if (!_created || !state)
            throw std::runtime_error("SDL2: combo box is not created.");
        state->visible = true;
        state->parent->invalidate();
    }

    void combo_box::destroy() const {
        if (!_created) return;
        auto *self = const_cast<combo_box *>(this);
        auto *state = linux::sdl2::combo_box_bindings
                          .object_from_handle(self);
        self->on_native_destroy();
        linux::sdl2::combo_boxes.erase(std::remove(
            linux::sdl2::combo_boxes.begin(),
            linux::sdl2::combo_boxes.end(), self),
            linux::sdl2::combo_boxes.end());
        if (state && state->parent) state->parent->invalidate();
        linux::sdl2::combo_box_bindings.unregister_by_handle(self);
        delete state;
    }
} // namespace native
