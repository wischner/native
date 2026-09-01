//
// Implements SDL2 collection and source-editor painting and dispatch.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>

#include <SDL2/SDL.h>

#include <native.h>

#include "../../collection_render.h"
#include "../../code_render.h"
#include "../../table_render.h"
#include "globals.h"

namespace
{
    native::wnd *root_of(native::wnd *control) {
        while (control && control->get_parent())
            control = control->get_parent();
        return control;
    }

    native::point origin_in_root(const native::wnd &control) {
        int x = control.get_position().x;
        int y = control.get_position().y;
        for (native::wnd *parent = control.get_parent();
             parent && parent->get_parent();
             parent = parent->get_parent()) {
            x += parent->get_position().x;
            y += parent->get_position().y;
        }
        return native::point(static_cast<native::coord>(x),
                             static_cast<native::coord>(y));
    }

    native::point local_point(native::wnd &control, int x, int y) {
        const native::point origin = origin_in_root(control);
        return native::point(static_cast<native::coord>(x - origin.x),
                             static_cast<native::coord>(y - origin.y));
    }

    bool hit(native::wnd &control, int x, int y) {
        const native::point origin = origin_in_root(control);
        return native::rect(origin, control.get_dimensions())
            .contains(native::point(x, y));
    }

    bool visible(native::accordion &control) {
        auto *state = linux::sdl2::accordion_bindings
                          .object_from_handle(&control);
        return state && state->visible && control.get_created();
    }

    bool visible(native::icon_view &control) {
        auto *state = linux::sdl2::icon_view_bindings
                          .object_from_handle(&control);
        return state && state->visible && control.get_created();
    }

    bool visible(native::tree_view &control) {
        auto *state = linux::sdl2::tree_view_bindings
                          .object_from_handle(&control);
        return state && state->visible && control.get_created();
    }

    bool visible(native::table_view &control) {
        auto *state = linux::sdl2::table_view_bindings
                          .object_from_handle(&control);
        return state && state->visible && control.get_created();
    }

    bool visible(native::code_edit &control) {
        auto *state = linux::sdl2::code_edit_bindings
                          .object_from_handle(&control);
        return state && state->visible && control.get_created();
    }

    void clear_focus(native::wnd *root) {
        for (auto *control : linux::sdl2::accordions) {
            if (control && root_of(control) == root)
                control->on_native_focus(false);
        }
        for (auto *control : linux::sdl2::icon_views) {
            if (control && root_of(control) == root)
                control->on_native_focus(false);
        }
        for (auto *control : linux::sdl2::tree_views) {
            if (control && root_of(control) == root)
                control->on_native_focus(false);
        }
        for (auto *control : linux::sdl2::table_views) {
            if (control && root_of(control) == root)
                control->on_native_focus(false);
        }
        for (auto *control : linux::sdl2::code_edits) {
            if (control && root_of(control) == root)
                control->on_native_focus(false);
        }
    }
} // namespace

namespace linux::sdl2
{
    void render_collections(native::wnd *owner, native::gpx &graphics) {
        for (auto *control : accordions) {
            if (control && visible(*control) && root_of(control) == owner)
                native::detail::draw_accordion_at(
                    *control, graphics, origin_in_root(*control));
        }
        for (auto *control : icon_views) {
            if (control && visible(*control) && root_of(control) == owner)
                native::detail::draw_icon_view_at(
                    *control, graphics, origin_in_root(*control));
        }
        for (auto *control : tree_views) {
            if (control && visible(*control) && root_of(control) == owner)
                native::detail::draw_tree_view_at(
                    *control, graphics, origin_in_root(*control));
        }
        for (auto *control : table_views) {
            if (control && visible(*control) && root_of(control) == owner)
                native::detail::draw_table_view_at(
                    *control, graphics, origin_in_root(*control));
        }
        for (auto *control : code_edits) {
            if (control && visible(*control) && root_of(control) == owner)
                native::draw_code_edit(
                    *control, graphics, origin_in_root(*control));
        }
    }

    bool handle_collection_mouse(native::wnd *owner,
                                 int x,
                                 int y,
                                 bool pressed,
                                 bool released,
                                 int clicks) {
        for (auto iterator = code_edits.rbegin();
             iterator != code_edits.rend(); ++iterator) {
            native::code_edit *control = *iterator;
            if (!control || !visible(*control) ||
                root_of(control) != owner || !hit(*control, x, y))
                continue;
            if (pressed) {
                clear_focus(owner);
                handle_text_edit_mouse(owner, x, y, true);
                control->on_native_focus(true);
                SDL_StartTextInput();
            }
            control->on_mouse_click.emit(native::mouse_event(
                native::mouse_button::left,
                pressed ? native::mouse_action::press
                        : native::mouse_action::release,
                local_point(*control, x, y)));
            return true;
        }
        for (auto iterator = table_views.rbegin();
             iterator != table_views.rend(); ++iterator) {
            native::table_view *control = *iterator;
            if (!control || !visible(*control) ||
                root_of(control) != owner || !hit(*control, x, y))
                continue;
            if (pressed) {
                clear_focus(owner);
                handle_text_edit_mouse(owner, x, y, true);
                control->on_native_focus(true);
                SDL_StartTextInput();
            }
            const native::point local = local_point(*control, x, y);
            control->on_mouse_click.emit(native::mouse_event(
                native::mouse_button::left,
                pressed ? native::mouse_action::press
                        : native::mouse_action::release,
                local));
            if (released && clicks >= 2) {
                const auto selected = control->get_selected_rows();
                if (!selected.empty())
                    control->on_native_activate(selected.back());
            }
            return true;
        }
        for (auto iterator = icon_views.rbegin();
             iterator != icon_views.rend();
             ++iterator) {
            native::icon_view *control = *iterator;
            if (!control || !visible(*control) ||
                root_of(control) != owner || !hit(*control, x, y))
                continue;
            if (pressed) {
                clear_focus(owner);
                control->on_native_focus(true);
            }
            const native::point local = local_point(*control, x, y);
            control->on_mouse_click.emit(native::mouse_event(
                native::mouse_button::left,
                pressed ? native::mouse_action::press
                        : native::mouse_action::release,
                local));
            if (released && clicks >= 2)
                control->on_native_activate(control->item_at(local));
            return true;
        }
        for (auto iterator = tree_views.rbegin();
             iterator != tree_views.rend();
             ++iterator) {
            native::tree_view *control = *iterator;
            if (!control || !visible(*control) ||
                root_of(control) != owner || !hit(*control, x, y))
                continue;
            if (pressed) {
                clear_focus(owner);
                control->on_native_focus(true);
            }
            const native::point local = local_point(*control, x, y);
            control->on_mouse_click.emit(native::mouse_event(
                native::mouse_button::left,
                pressed ? native::mouse_action::press
                        : native::mouse_action::release,
                local));
            if (released && clicks >= 2) {
                const native::tree_view_hit hit =
                    control->hit_test(local);
                if (hit.part == native::tree_view_hit_part::row)
                    control->on_native_double_click(hit.id);
            }
            return true;
        }
        for (auto iterator = accordions.rbegin();
             iterator != accordions.rend();
             ++iterator) {
            native::accordion *control = *iterator;
            if (!control || !visible(*control) ||
                root_of(control) != owner || !hit(*control, x, y))
                continue;
            if (pressed) {
                clear_focus(owner);
                control->on_native_focus(true);
            }
            control->on_mouse_click.emit(native::mouse_event(
                native::mouse_button::left,
                pressed ? native::mouse_action::press
                        : native::mouse_action::release,
                local_point(*control, x, y)));
            return true;
        }
        return false;
    }

    bool handle_collection_wheel(native::wnd *owner,
                                 int x,
                                 int y,
                                 int delta) {
        for (auto iterator = code_edits.rbegin();
             iterator != code_edits.rend(); ++iterator) {
            native::code_edit *control = *iterator;
            if (control && visible(*control) &&
                root_of(control) == owner && hit(*control, x, y)) {
                control->on_mouse_wheel.emit(
                    native::mouse_wheel_event(
                        local_point(*control, x, y),
                        static_cast<native::coord>(delta),
                        native::wheel_direction::vertical));
                return true;
            }
        }
        for (auto iterator = table_views.rbegin();
             iterator != table_views.rend(); ++iterator) {
            native::table_view *control = *iterator;
            if (control && visible(*control) &&
                root_of(control) == owner && hit(*control, x, y)) {
                const std::size_t current =
                    control->get_vertical_scroll_row();
                const std::size_t amount =
                    static_cast<std::size_t>(std::max(1, std::abs(delta)));
                control->on_native_scroll(
                    delta > 0 ? current - std::min(current, amount)
                              : current + amount,
                    control->get_horizontal_scroll_offset());
                return true;
            }
        }
        for (auto iterator = icon_views.rbegin();
             iterator != icon_views.rend();
             ++iterator) {
            native::icon_view *control = *iterator;
            if (control && visible(*control) &&
                root_of(control) == owner && hit(*control, x, y)) {
                control->on_native_scroll(-delta);
                return true;
            }
        }
        for (auto iterator = tree_views.rbegin();
             iterator != tree_views.rend();
             ++iterator) {
            native::tree_view *control = *iterator;
            if (control && visible(*control) &&
                root_of(control) == owner && hit(*control, x, y)) {
                control->on_native_scroll(-delta);
                return true;
            }
        }
        return false;
    }

    bool handle_collection_motion(native::wnd *owner, int x, int y) {
        for (auto iterator = code_edits.rbegin();
             iterator != code_edits.rend(); ++iterator) {
            native::code_edit *control = *iterator;
            if (control && visible(*control) &&
                root_of(control) == owner && hit(*control, x, y)) {
                control->on_mouse_move.emit(
                    local_point(*control, x, y));
                return true;
            }
        }
        return false;
    }

    bool handle_collection_key(native::wnd *owner,
                               const SDL_KeyboardEvent &event) {
        for (auto *control : code_edits) {
            if (!control || !visible(*control) ||
                root_of(control) != owner || !control->get_focused())
                continue;
            const bool extend =
                (event.keysym.mod & KMOD_SHIFT) != 0;
            const bool command =
                (event.keysym.mod & KMOD_CTRL) != 0;
            if (command) {
                native::code_edit_key key;
                bool handled = true;
                if (event.keysym.sym == SDLK_a)
                    key = native::code_edit_key::select_all;
                else if (event.keysym.sym == SDLK_c)
                    key = native::code_edit_key::copy;
                else if (event.keysym.sym == SDLK_x)
                    key = native::code_edit_key::cut;
                else if (event.keysym.sym == SDLK_v)
                    key = native::code_edit_key::paste;
                else if (event.keysym.sym == SDLK_z)
                    key = extend ? native::code_edit_key::redo
                                 : native::code_edit_key::undo;
                else
                    handled = false;
                if (handled)
                    control->on_native_key(key);
                return handled;
            }
            native::code_edit_key key;
            bool handled = true;
            switch (event.keysym.sym) {
            case SDLK_LEFT:
                key = native::code_edit_key::left;
                break;
            case SDLK_RIGHT:
                key = native::code_edit_key::right;
                break;
            case SDLK_UP:
                key = native::code_edit_key::up;
                break;
            case SDLK_DOWN:
                key = native::code_edit_key::down;
                break;
            case SDLK_HOME:
                key = native::code_edit_key::home;
                break;
            case SDLK_END:
                key = native::code_edit_key::end;
                break;
            case SDLK_PAGEUP:
                key = native::code_edit_key::page_up;
                break;
            case SDLK_PAGEDOWN:
                key = native::code_edit_key::page_down;
                break;
            case SDLK_BACKSPACE:
                key = native::code_edit_key::backspace;
                break;
            case SDLK_DELETE:
                key = native::code_edit_key::delete_forward;
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                key = native::code_edit_key::enter;
                break;
            case SDLK_TAB:
                key = native::code_edit_key::tab;
                break;
            case SDLK_ESCAPE:
                key = native::code_edit_key::escape;
                break;
            default:
                handled = false;
                break;
            }
            if (handled)
                control->on_native_key(key, extend);
            return handled;
        }
        for (auto *control : table_views) {
            if (!control || !visible(*control) ||
                root_of(control) != owner || !control->get_focused())
                continue;
            const bool extend =
                (event.keysym.mod & KMOD_SHIFT) != 0;
            if ((event.keysym.mod & KMOD_CTRL) != 0 &&
                event.keysym.sym == SDLK_a) {
                control->on_native_navigation(
                    native::table_navigation::select_all);
                return true;
            }
            switch (event.keysym.sym) {
            case SDLK_UP:
                control->on_native_navigation(
                    native::table_navigation::up, extend);
                break;
            case SDLK_DOWN:
                control->on_native_navigation(
                    native::table_navigation::down, extend);
                break;
            case SDLK_HOME:
                control->on_native_navigation(
                    native::table_navigation::home, extend);
                break;
            case SDLK_END:
                control->on_native_navigation(
                    native::table_navigation::end, extend);
                break;
            case SDLK_PAGEUP:
                control->on_native_navigation(
                    native::table_navigation::page_up, extend);
                break;
            case SDLK_PAGEDOWN:
                control->on_native_navigation(
                    native::table_navigation::page_down, extend);
                break;
            case SDLK_LEFT:
                control->on_native_navigation(
                    native::table_navigation::collapse);
                break;
            case SDLK_RIGHT:
                control->on_native_navigation(
                    native::table_navigation::expand);
                break;
            case SDLK_SPACE:
                control->on_native_navigation(
                    native::table_navigation::toggle);
                break;
            case SDLK_RETURN:
                control->on_native_navigation(
                    native::table_navigation::activate);
                break;
            default:
                return false;
            }
            return true;
        }
        for (auto *control : accordions) {
            if (!control || !visible(*control) ||
                root_of(control) != owner ||
                control->get_focused_index() < 0)
                continue;
            switch (event.keysym.sym) {
            case SDLK_UP:
                control->on_native_navigation(
                    native::accordion_navigation::previous);
                break;
            case SDLK_DOWN:
                control->on_native_navigation(
                    native::accordion_navigation::next);
                break;
            case SDLK_HOME:
                control->on_native_navigation(
                    native::accordion_navigation::first);
                break;
            case SDLK_END:
                control->on_native_navigation(
                    native::accordion_navigation::last);
                break;
            case SDLK_RETURN:
            case SDLK_SPACE:
                control->on_native_navigation(
                    native::accordion_navigation::toggle);
                break;
            default:
                return false;
            }
            return true;
        }
        for (auto *control : icon_views) {
            if (!control || !visible(*control) ||
                root_of(control) != owner || !control->get_focused())
                continue;
            switch (event.keysym.sym) {
            case SDLK_LEFT:
                control->on_native_navigation(
                    native::icon_view_navigation::left);
                break;
            case SDLK_RIGHT:
                control->on_native_navigation(
                    native::icon_view_navigation::right);
                break;
            case SDLK_UP:
                control->on_native_navigation(
                    native::icon_view_navigation::up);
                break;
            case SDLK_DOWN:
                control->on_native_navigation(
                    native::icon_view_navigation::down);
                break;
            case SDLK_HOME:
                control->on_native_navigation(
                    native::icon_view_navigation::home);
                break;
            case SDLK_END:
                control->on_native_navigation(
                    native::icon_view_navigation::end);
                break;
            case SDLK_PAGEUP:
                control->on_native_navigation(
                    native::icon_view_navigation::page_up);
                break;
            case SDLK_PAGEDOWN:
                control->on_native_navigation(
                    native::icon_view_navigation::page_down);
                break;
            case SDLK_RETURN:
                control->on_native_activate(
                    control->get_selected_index());
                break;
            default:
                return false;
            }
            return true;
        }
        for (auto *control : tree_views) {
            if (!control || !visible(*control) ||
                root_of(control) != owner || !control->get_focused())
                continue;
            switch (event.keysym.sym) {
            case SDLK_UP:
                control->on_native_navigation(
                    native::tree_view_navigation::up);
                break;
            case SDLK_DOWN:
                control->on_native_navigation(
                    native::tree_view_navigation::down);
                break;
            case SDLK_LEFT:
                control->on_native_navigation(
                    native::tree_view_navigation::left);
                break;
            case SDLK_RIGHT:
                control->on_native_navigation(
                    native::tree_view_navigation::right);
                break;
            case SDLK_HOME:
                control->on_native_navigation(
                    native::tree_view_navigation::home);
                break;
            case SDLK_END:
                control->on_native_navigation(
                    native::tree_view_navigation::end);
                break;
            case SDLK_PAGEUP:
                control->on_native_navigation(
                    native::tree_view_navigation::page_up);
                break;
            case SDLK_PAGEDOWN:
                control->on_native_navigation(
                    native::tree_view_navigation::page_down);
                break;
            case SDLK_SPACE:
                control->on_native_navigation(
                    native::tree_view_navigation::toggle);
                break;
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                control->on_native_navigation(
                    native::tree_view_navigation::activate);
                break;
            default:
                return false;
            }
            return true;
        }
        return false;
    }

    bool handle_collection_text(native::wnd *owner,
                                const char *text) {
        for (auto *control : code_edits) {
            if (control && visible(*control) &&
                root_of(control) == owner && control->get_focused()) {
                control->on_native_text_input(text ? text : "");
                return true;
            }
        }
        for (auto *control : table_views) {
            if (control && visible(*control) &&
                root_of(control) == owner && control->get_focused()) {
                control->on_native_type_text(text ? text : "");
                return true;
            }
        }
        return false;
    }
} // namespace linux::sdl2

namespace native
{
    void accordion::apply_items() { invalidate(); }

    void accordion::create() const {
        if (_created)
            return;
        if (!get_parent() || !get_parent()->get_created())
            throw std::runtime_error(
                "SDL2: accordion requires a created parent.");
        auto *self = const_cast<accordion *>(this);
        linux::sdl2::accordion_bindings.register_pair(
            self, new linux::sdl2::sdl2_collection());
        linux::sdl2::accordions.push_back(self);
        _created = true;
        self->synchronize_theme_metrics();
        self->refresh();
        self->on_wnd_create.emit();
    }

    void accordion::show() const {
        auto *state = linux::sdl2::accordion_bindings
                          .object_from_handle(
                              const_cast<accordion *>(this));
        if (!_created || !state)
            throw std::runtime_error("SDL2: accordion is not created.");
        state->visible = true;
        invalidate();
    }

    void accordion::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<accordion *>(this);
        auto *state = linux::sdl2::accordion_bindings
                          .object_from_handle(self);
        self->on_native_destroy();
        linux::sdl2::accordions.erase(
            std::remove(linux::sdl2::accordions.begin(),
                        linux::sdl2::accordions.end(),
                        self),
            linux::sdl2::accordions.end());
        linux::sdl2::accordion_bindings.unregister_by_handle(self);
        delete state;
    }

    void icon_view::apply_items() { invalidate(); }
    void icon_view::apply_icon_size() { invalidate(); }
    void icon_view::apply_label_mode() { invalidate(); }
    void icon_view::apply_selected_index() { invalidate(); }
    void icon_view::apply_scroll_offset() { invalidate(); }

    void icon_view::create() const {
        if (_created)
            return;
        if (!get_parent() || !get_parent()->get_created())
            throw std::runtime_error(
                "SDL2: icon_view requires a created parent.");
        auto *self = const_cast<icon_view *>(this);
        linux::sdl2::icon_view_bindings.register_pair(
            self, new linux::sdl2::sdl2_collection());
        linux::sdl2::icon_views.push_back(self);
        _created = true;
        self->synchronize_theme_metrics();
        self->on_wnd_create.emit();
    }

    void icon_view::show() const {
        auto *state = linux::sdl2::icon_view_bindings
                          .object_from_handle(
                              const_cast<icon_view *>(this));
        if (!_created || !state)
            throw std::runtime_error("SDL2: icon_view is not created.");
        state->visible = true;
        invalidate();
    }

    void icon_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<icon_view *>(this);
        auto *state = linux::sdl2::icon_view_bindings
                          .object_from_handle(self);
        self->on_native_destroy();
        linux::sdl2::icon_views.erase(
            std::remove(linux::sdl2::icon_views.begin(),
                        linux::sdl2::icon_views.end(),
                        self),
            linux::sdl2::icon_views.end());
        linux::sdl2::icon_view_bindings.unregister_by_handle(self);
        delete state;
    }

} // namespace native
