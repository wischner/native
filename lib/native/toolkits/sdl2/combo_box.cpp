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
    bool handle_combo_mouse(native::wnd *owner,
                            int x,
                            int y,
                            bool pressed,
                            bool released) {
        if (pressed) {
            // Popups paint above every ordinary control, so they must also
            // win hit testing when their rows overlap another combo box.
            for (auto iterator = combo_boxes.rbegin();
                 iterator != combo_boxes.rend(); ++iterator) {
                native::combo_box *combo = *iterator;
                auto *state =
                    combo_box_bindings.object_from_handle(combo);
                if (!state || root_of(combo) != owner ||
                    !state->visible || !state->open)
                    continue;
                const native::rect bounds = root_bounds(*combo);
                const int item_height =
                    native::theme::metrics{}.list_item_height;
                const native::rect popup = popup_bounds(
                    *owner, *combo, bounds, item_height);
                const native::point position(
                    static_cast<native::coord>(x),
                    static_cast<native::coord>(y));
                if (popup.contains(position)) {
                    const int index = (y - popup.y1()) / item_height;
                    if (auto *window_state =
                            wnd_gpx_bindings.object_from_handle(owner)) {
                        window_state->suppress_combo_release = true;
                    }
                    state->open = false;
                    state->focused = true;
                    state->hovered_index = -1;
                    combo->on_native_drop_down(false);
                    if (still_registered(combo) && index >= 0 &&
                        index < static_cast<int>(
                            combo->get_items().size())) {
                        combo->on_native_selection(index);
                    }
                    return true;
                }
                if (!bounds.contains(position)) {
                    state->open = false;
                    state->focused = false;
                    state->hovered_index = -1;
                    combo->on_native_drop_down(false);
                }
                return true;
            }

            for (auto iterator = combo_boxes.rbegin();
                 iterator != combo_boxes.rend(); ++iterator) {
                native::combo_box *combo = *iterator;
                auto *state =
                    combo_box_bindings.object_from_handle(combo);
                if (!state || root_of(combo) != owner ||
                    !state->visible || state->open)
                    continue;
                const native::rect bounds = root_bounds(*combo);
                if (bounds.contains({
                        static_cast<native::coord>(x),
                        static_cast<native::coord>(y)}))
                    return true;
            }
            return false;
        }
        if (!released)
            return false;
        if (auto *window_state =
                wnd_gpx_bindings.object_from_handle(owner);
            window_state && window_state->suppress_combo_release) {
            window_state->suppress_combo_release = false;
            return true;
        }
        bool changed = false;
        for (auto iterator = combo_boxes.rbegin();
             iterator != combo_boxes.rend(); ++iterator) {
            native::combo_box *combo = *iterator;
            auto *state = combo_box_bindings.object_from_handle(combo);
            if (!state || root_of(combo) != owner || !state->visible)
                continue;
            const native::rect bounds = root_bounds(*combo);
            const int item_height = native::theme::metrics{}.list_item_height;
            const native::rect popup =
                popup_bounds(*owner, *combo, bounds, item_height);
            if (state->open && popup.contains({
                    static_cast<native::coord>(x),
                    static_cast<native::coord>(y)})) {
                const int index = (y-popup.y1())/item_height;
                state->open = false;
                state->hovered_index = -1;
                combo->on_native_drop_down(false);
                if (still_registered(combo) && index >= 0 &&
                    index < static_cast<int>(combo->get_items().size()))
                    combo->on_native_selection(index);
                return true;
            } else if (bounds.contains({static_cast<native::coord>(x),
                                        static_cast<native::coord>(y)})) {
                state->focused = true;
                state->open = !state->open;
                state->hovered_index = -1;
                const bool open = state->open;
                combo->on_native_drop_down(open);
                if (still_registered(combo) &&
                    combo->get_style() ==
                        native::combo_box_style::editable) {
                    SDL_StartTextInput();
                }
                return true;
            } else {
                if (state->open) {
                    state->open = false;
                    state->hovered_index = -1;
                    combo->on_native_drop_down(false);
                    // Avoid retaining an iterator across user callbacks
                    // which may mutate the registry.
                    return true;
                }
                state->open = false;
                state->hovered_index = -1;
                changed = changed || state->focused;
                state->focused = false;
            }
        }
        if (changed)
            owner->invalidate();
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
        for (auto *combo : combo_boxes) {
            auto *state = combo_box_bindings.object_from_handle(combo);
            if (!state || root_of(combo) != owner ||
                !state->focused || !state->visible) continue;
            if (event.keysym.sym == SDLK_BACKSPACE &&
                combo->get_style() == native::combo_box_style::editable &&
                !combo->get_text().empty()) {
                std::string text = combo->get_text();
                text.pop_back();
                combo->on_native_text(text);
                if (still_registered(combo) && owner->get_created())
                    owner->invalidate();
            } else if (event.keysym.sym == SDLK_ESCAPE && state->open) {
                state->open = false;
                state->hovered_index = -1;
                combo->on_native_drop_down(false);
                if (still_registered(combo) && owner->get_created())
                    owner->invalidate();
            } else if (event.keysym.sym == SDLK_DOWN ||
                       event.keysym.sym == SDLK_UP) {
                if (combo->get_items().empty())
                    return true;
                int index = combo->get_selected_index();
                index += event.keysym.sym == SDLK_DOWN ? 1 : -1;
                index = std::clamp(index, 0,
                    static_cast<int>(combo->get_items().size())-1);
                state->hovered_index = -1;
                combo->on_native_selection(index);
                if (still_registered(combo) && owner->get_created())
                    owner->invalidate();
            }
            return true;
        }
        return false;
    }

    bool handle_combo_text(native::wnd *owner, const char *value) {
        for (auto *combo : combo_boxes) {
            auto *state = combo_box_bindings.object_from_handle(combo);
            if (!state || root_of(combo) != owner ||
                !state->focused || !state->visible ||
                combo->get_style() != native::combo_box_style::editable)
                continue;
            combo->on_native_text(combo->get_text()+
                                  (value ? value : ""));
            if (still_registered(combo) && owner->get_created())
                owner->invalidate();
            return true;
        }
        return false;
    }

    void render_combo_boxes(native::wnd *owner, native::gpx &graphics) {
        auto appearance = native::theme::create(graphics);
        for (auto *combo : combo_boxes) {
            auto *state = combo_box_bindings.object_from_handle(combo);
            if (!state || root_of(combo) != owner || !state->visible) continue;
            native::theme::state control_state;
            control_state.focused = state->focused;
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
    void combo_box::apply_text() { invalidate(); }
    void combo_box::apply_style() { invalidate(); }

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
