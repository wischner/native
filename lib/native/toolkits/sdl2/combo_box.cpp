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

namespace
{
    native::rect popup_bounds(native::wnd &owner,
                              native::combo_box &combo,
                              const native::rect &bounds,
                              int row_height) {
        const int desired = static_cast<int>(combo.get_items().size()) *
                            std::max(1, row_height);
        const int below = std::max(
            0, static_cast<int>(owner.get_dimensions().h) - bounds.y2());
        const int above = std::max(0, static_cast<int>(bounds.y1()));
        const bool open_above = desired > below && above > below;
        const int height = std::min(desired, open_above ? above : below);
        const int y = open_above ? bounds.y1() - height : bounds.y2();
        return native::rect(
            bounds.x1(),
            static_cast<native::coord>(y),
            bounds.w(),
            static_cast<native::dim>(std::max(0, height)));
    }

    bool still_registered(native::combo_box *combo) {
        return std::find(linux::sdl2::combo_boxes.begin(),
                         linux::sdl2::combo_boxes.end(),
                         combo) != linux::sdl2::combo_boxes.end();
    }
} // namespace

namespace linux::sdl2
{
    bool handle_combo_mouse(
        native::wnd *owner, int x, int y, bool pressed, bool released) {
        if (!pressed && !released)
            return false;
        auto *window = wnd_gpx_bindings.object_from_handle(owner);
        if (released && window && window->suppress_combo_release) {
            window->suppress_combo_release = false;
            return true;
        }
        const native::point point(x, y);
        // Transient content wins over all sibling fields.
        for (auto *combo : combo_boxes) {
            auto *state = combo_box_bindings.object_from_handle(combo);
            if (!state || root_of(combo) != owner || !state->visible ||
                !state->open)
                continue;
            const auto bounds = root_bounds(*combo);
            if (bounds.contains(point))
                break;
            if (!pressed)
                return true;
            if (window)
                window->suppress_combo_release = true;
            if (state->popup_click) {
                auto click = state->popup_click;
                click(x, y);
                return true;
            }
            const auto popup =
                popup_bounds(*owner,
                             *combo,
                             bounds,
                             native::theme::create(owner->get_gpx())
                                 ->get_list_item_height());
            state->open = false;
            state->hovered_index = -1;
            combo->on_native_drop_down(false);
            if (still_registered(combo) && popup.contains(point)) {
                const int index =
                    (y - popup.y1() - 1) /
                    std::max(1,
                             native::theme::create(owner->get_gpx())
                                 ->get_list_item_height());
                if (index >= 0 &&
                    index < int(combo->get_items().size())) {
                    combo->on_native_selection(index);
                    if (still_registered(combo)) {
                        update_combo_editor(*combo);
                        auto *peer =
                            combo_box_bindings.object_from_handle(
                                combo);
                        if (peer->editor)
                            peer->editor->on_native_focus(true);
                    }
                }
            }
            return true;
        }
        for (auto iterator = combo_boxes.rbegin();
             iterator != combo_boxes.rend();
             ++iterator) {
            auto *combo = *iterator;
            auto *state = combo_box_bindings.object_from_handle(combo);
            if (!state || root_of(combo) != owner || !state->visible)
                continue;
            update_combo_editor(*combo);
            const auto bounds = root_bounds(*combo);
            if (!bounds.contains(point))
                continue;
            const int button = combo_button_width(*state, bounds);
            const bool arrow = x >= bounds.x2() - button;
            if (!arrow && state->editor)
                return false;
            if (pressed) {
                combo->on_native_focus(true);
                return true;
            }
            if (state->step) {
                auto step = state->step;
                step(y < bounds.y1() + int(bounds.h()) / 2 ? 1 : -1);
                return true;
            }
            state->open = !state->open;
            state->hovered_index = -1;
            combo->on_native_drop_down(state->open);
            return true;
        }
        return false;
    }

    bool handle_combo_motion(native::wnd *owner, int x, int y) {
        for (auto iterator = combo_boxes.rbegin();
             iterator != combo_boxes.rend(); ++iterator) {
            native::combo_box *combo = *iterator;
            auto *state = combo_box_bindings.object_from_handle(combo);
            if (!state || root_of(combo) != owner || !state->visible ||
                !state->open) {
                continue;
            }
            const native::rect bounds = root_bounds(*combo);
            const int item_height =
                native::theme::metrics{}.list_item_height;
            const native::rect popup = popup_bounds(
                *owner, *combo, bounds, item_height);
            int hovered_index = -1;
            if (popup.contains({static_cast<native::coord>(x),
                                static_cast<native::coord>(y)})) {
                const int candidate = (y - popup.y1()) / item_height;
                if (candidate >= 0 &&
                    candidate < static_cast<int>(
                        combo->get_items().size())) {
                    hovered_index = candidate;
                }
            }
            if (state->hovered_index != hovered_index) {
                state->hovered_index = hovered_index;
                if (owner->get_created())
                    owner->invalidate();
            }
            return true;
        }
        return false;
    }

    bool handle_combo_key(native::wnd *owner,
                          const SDL_KeyboardEvent &event) {
        if (event.type != SDL_KEYDOWN) return false;
        auto *focus = native::detail::input_target(*owner);
        for (auto *combo : combo_boxes) {
            auto *state = combo_box_bindings.object_from_handle(combo);
            if (!state || root_of(combo) != owner || !state->visible ||
                (focus != combo && focus != state->editor.get()))
                continue;
            const auto key = event.keysym.sym;
            if ((key == SDLK_RETURN || key == SDLK_KP_ENTER) &&
                state->commit) {
                auto commit = state->commit;
                commit();
                return true;
            }
            if ((key == SDLK_ESCAPE || key == SDLK_RETURN ||
                 key == SDLK_KP_ENTER) &&
                state->open) {
                state->open = false;
                combo->on_native_drop_down(false);
                return true;
            }
            if (state->step && (key == SDLK_UP || key == SDLK_DOWN)) {
                auto step = state->step;
                step(key == SDLK_UP ? 1 : -1);
                return true;
            }
            if (state->editor && !state->open) {
                if (key != SDLK_DOWN || !(event.keysym.mod & KMOD_ALT))
                    return false;
                state->open = true;
                combo->on_native_drop_down(true);
                return true;
            }
            if (key == SDLK_DOWN || key == SDLK_UP) {
                if (combo->get_items().empty())
                    return true;
                const int index =
                    std::clamp(combo->get_selected_index() +
                                   (key == SDLK_DOWN ? 1 : -1),
                               0,
                               int(combo->get_items().size()) - 1);
                combo->on_native_selection(index);
                if (still_registered(combo))
                    update_combo_editor(*combo);
                return true;
            }
            return false;
        }
        return false;
    }

    bool handle_combo_text(native::wnd *, const char *) {
        // Editable text, selection and clipboard use text_edit.
        return false;
    }

    void render_combo_boxes(native::wnd *owner, native::gpx &graphics) {
        auto appearance = native::theme::create(graphics);
        for (auto *combo : combo_boxes) {
            auto *state = combo_box_bindings.object_from_handle(combo);
            if (!state || root_of(combo) != owner || !state->visible) continue;
            update_combo_editor(*combo);
            native::theme::state control_state;
            control_state.focused =
                native::detail::input_target(*owner) == combo;
            control_state.pressed = state->open;
            const native::rect bounds = root_bounds(*combo);
            native::detail::control_render_access::draw(
                *combo, graphics, *appearance,
                bounds, control_state);
        }
    }

    void render_combo_popups(native::wnd *owner,
                             native::gpx &graphics) {
        auto appearance = native::theme::create(graphics);
        const int row_height = appearance->defaults().list_item_height;
        for (auto *combo : combo_boxes) {
            auto *state = combo_box_bindings.object_from_handle(combo);
            if (!state || root_of(combo) != owner || !state->visible ||
                !state->open)
                continue;
            if (state->popup_paint) {
                state->popup_paint(graphics);
                continue;
            }
            const native::rect bounds = root_bounds(*combo);
            const native::rect popup =
                popup_bounds(*owner, *combo, bounds, row_height);
            if (!popup.d.h)
                continue;
            appearance->draw_popup_frame(popup);
            const native::rect content(
                static_cast<native::coord>(popup.x1() + 1),
                static_cast<native::coord>(popup.y1() + 1),
                static_cast<native::dim>(
                    std::max(0, static_cast<int>(popup.w()) - 2)),
                static_cast<native::dim>(
                    std::max(0, static_cast<int>(popup.h()) - 2)));
            for (std::size_t index = 0;
                 index < combo->get_items().size(); ++index) {
                const int y = content.y1() +
                              static_cast<int>(index) * row_height;
                if (y >= content.y2())
                    break;
                native::theme::state item_state;
                item_state.hot =
                    static_cast<int>(index) == state->hovered_index;
                item_state.selected = state->hovered_index < 0 &&
                    static_cast<int>(index) == combo->get_selected_index();
                appearance->draw_list_item(
                    native::rect(content.x1(),
                        static_cast<native::coord>(y),
                        content.w(),
                        static_cast<native::dim>(std::min(
                            row_height, content.y2() - y))),
                    combo->get_items()[index], item_state);
            }
            graphics.set_pen(1)
                .set_ink(appearance->get_menu_popup_border_color())
                .draw_rect(popup, false);
        }
    }
} // namespace linux::sdl2

namespace native
{
    void combo_box::apply_items() { invalidate(); }
    void combo_box::apply_selected_index() { invalidate(); }
    void combo_box::apply_text() {
        linux::sdl2::update_combo_editor(*this);
        invalidate();
    }
    void combo_box::apply_style() {
        linux::sdl2::update_combo_editor(*this);
        invalidate();
    }

    void combo_box::create_native() {
        wnd *parent = get_parent();
        if (!parent || !parent->get_created())
            throw std::runtime_error(
                "SDL2: combo box requires a created parent.");
        auto *self = this;
        auto *state = new linux::sdl2::sdl2_combo_box;
        state->parent = parent;
        linux::sdl2::combo_box_bindings.register_pair(self, state);
        linux::sdl2::combo_boxes.push_back(self);
    }

    void combo_box::show_native() {
        auto *state = linux::sdl2::combo_box_bindings.object_from_handle(
            this);
        if (!_created || !state)
            throw std::runtime_error("SDL2: combo box is not created.");
        state->visible = true;
        linux::sdl2::update_combo_editor(*this);
        if (state->editor)
            state->editor->show();
        state->parent->invalidate();
    }

    void combo_box::destroy_native() {
        if (!_created) return;
        auto *self = this;
        auto *state = linux::sdl2::combo_box_bindings
                          .object_from_handle(self);
        linux::sdl2::combo_boxes.erase(std::remove(
            linux::sdl2::combo_boxes.begin(),
            linux::sdl2::combo_boxes.end(), self),
            linux::sdl2::combo_boxes.end());
        if (state && state->parent) state->parent->invalidate();
        linux::sdl2::combo_box_bindings.unregister_by_handle(self);
        delete state;
    }
} // namespace native
