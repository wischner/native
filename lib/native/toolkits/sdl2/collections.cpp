//
// Implements SDL2 collection and source-editor painting and dispatch.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include <SDL2/SDL.h>

#include <native.h>

#include "../../collection_render.h"
#include "../../classic_scrollbar.h"
#include "../../code_render.h"
#include "../../table_render.h"
#include "globals.h"

namespace
{
    using linux::sdl2::origin_in_root;
    using linux::sdl2::root_of;

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

    bool visible(native::tab_view &control) {
        auto *state = linux::sdl2::tab_view_bindings
                          .object_from_handle(&control);
        return state && state->visible && control.get_created();
    }

    bool visible(native::split_view &control) {
        auto *state = linux::sdl2::split_view_bindings
                          .object_from_handle(&control);
        return state && state->visible && control.get_created();
    }

    native::split_view *active_splitter = nullptr;

    struct collection_scrollbar
    {
        native::detail::classic_scrollbar_geometry geometry;
        std::uint64_t total = 0;
        std::uint64_t page = 0;
        int step = 1;
    };

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

    collection_scrollbar icon_scrollbar(native::icon_view &control) {
        const native::theme::metrics metrics;
        collection_scrollbar result;
        result.total = control.get_content_dimensions().h;
        result.page = control.get_dimensions().h;
        result.step = std::max(1, metrics.list_item_height);
        const int extent = std::max(1, metrics.scrollbar_extent);
        const native::rect bounds(
            static_cast<native::coord>(std::max(
                0,
                static_cast<int>(control.get_dimensions().w) - extent)),
            0,
            static_cast<native::dim>(extent),
            control.get_dimensions().h);
        result.geometry = native::detail::make_classic_scrollbar(
            bounds,
            native::scrollbar_orientation::vertical,
            result.total,
            result.page,
            static_cast<std::uint64_t>(control.get_scroll_offset()),
            metrics.scrollbar_min_thumb);
        return result;
    }

    collection_scrollbar tree_scrollbar(native::tree_view &control) {
        const native::theme::metrics metrics;
        collection_scrollbar result;
        const int row_height = control.get_visible_item_count() > 0
            ? static_cast<int>(control.get_row_bounds(0).d.h)
            : std::max(1, metrics.list_item_height);
        result.total = static_cast<std::uint64_t>(
            control.get_visible_item_count()) *
            static_cast<std::uint64_t>(std::max(1, row_height));
        result.page = control.get_dimensions().h;
        result.step = std::max(1, row_height);
        const int extent = std::max(1, metrics.scrollbar_extent);
        const native::rect bounds(
            static_cast<native::coord>(std::max(
                0,
                static_cast<int>(control.get_dimensions().w) - extent)),
            0,
            static_cast<native::dim>(extent),
            control.get_dimensions().h);
        result.geometry = native::detail::make_classic_scrollbar(
            bounds,
            native::scrollbar_orientation::vertical,
            result.total,
            result.page,
            static_cast<std::uint64_t>(control.get_scroll_offset()),
            metrics.scrollbar_min_thumb);
        return result;
    }

    bool begin_scrollbar_pointer(
        native::collection_view &control,
        linux::sdl2::sdl2_collection &state,
        const collection_scrollbar &scrollbar,
        native::point position) {
        if (scrollbar.total <= scrollbar.page ||
            !scrollbar.geometry.bounds.contains(position)) {
            return false;
        }
        state.scrollbar_pointer_active = true;
        if (scrollbar.geometry.thumb.contains(position)) {
            state.scrollbar_dragging = true;
            state.scrollbar_grab_offset =
                position.y - scrollbar.geometry.thumb.y1();
            return true;
        }
        std::int64_t offset = control.get_scroll_offset();
        if (scrollbar.geometry.decrement.contains(position)) {
            offset -= scrollbar.step;
        } else if (scrollbar.geometry.increment.contains(position)) {
            offset += scrollbar.step;
        } else if (position.y < scrollbar.geometry.thumb.y1()) {
            offset -= static_cast<std::int64_t>(std::min<std::uint64_t>(
                scrollbar.page,
                static_cast<std::uint64_t>(
                    std::numeric_limits<int>::max())));
        } else if (position.y >= scrollbar.geometry.thumb.y2()) {
            offset += static_cast<std::int64_t>(std::min<std::uint64_t>(
                scrollbar.page,
                static_cast<std::uint64_t>(
                    std::numeric_limits<int>::max())));
        }
        control.set_scroll_offset(static_cast<int>(std::clamp<std::int64_t>(
            offset,
            0,
            std::numeric_limits<int>::max())));
        return true;
    }

    bool drag_scrollbar_pointer(
        native::collection_view &control,
        linux::sdl2::sdl2_collection &state,
        const collection_scrollbar &scrollbar,
        native::point position) {
        if (!state.scrollbar_dragging)
            return false;
        const std::uint64_t value =
            native::detail::classic_scrollbar_drag_value(
                scrollbar.geometry,
                native::scrollbar_orientation::vertical,
                position.y,
                state.scrollbar_grab_offset,
                scrollbar.total,
                scrollbar.page);
        control.set_scroll_offset(static_cast<int>(
            std::min<std::uint64_t>(
                value,
                static_cast<std::uint64_t>(
                    std::numeric_limits<int>::max()))));
        return true;
    }

    bool finish_scrollbar_pointer(
        native::wnd *owner,
        int x,
        int y) {
        for (auto *control : linux::sdl2::table_views) {
            auto *state = control
                ? linux::sdl2::table_view_bindings.object_from_handle(control)
                : nullptr;
            if (!state || !state->scrollbar_pointer_active ||
                root_of(control) != owner) {
                continue;
            }
            native::detail::drag_table_scrollbar(
                *control,
                local_point(*control, x, y),
                state->scrollbar_horizontal,
                state->scrollbar_grab_offset);
            state->scrollbar_pointer_active = false;
            state->scrollbar_dragging = false;
            return true;
        }
        for (auto *control : linux::sdl2::icon_views) {
            auto *state = control
                ? linux::sdl2::icon_view_bindings.object_from_handle(control)
                : nullptr;
            if (!state || !state->scrollbar_pointer_active ||
                root_of(control) != owner) {
                continue;
            }
            drag_scrollbar_pointer(
                *control,
                *state,
                icon_scrollbar(*control),
                local_point(*control, x, y));
            state->scrollbar_pointer_active = false;
            state->scrollbar_dragging = false;
            return true;
        }
        for (auto *control : linux::sdl2::tree_views) {
            auto *state = control
                ? linux::sdl2::tree_view_bindings.object_from_handle(control)
                : nullptr;
            if (!state || !state->scrollbar_pointer_active ||
                root_of(control) != owner) {
                continue;
            }
            drag_scrollbar_pointer(
                *control,
                *state,
                tree_scrollbar(*control),
                local_point(*control, x, y));
            state->scrollbar_pointer_active = false;
            state->scrollbar_dragging = false;
            return true;
        }
        return false;
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
    void render_tab_views(native::wnd *owner, native::gpx &graphics) {
        for (auto *control : split_views) {
            if (control && visible(*control) && root_of(control) == owner)
                native::detail::draw_split_view_at(
                    *control, graphics, origin_in_root(*control));
        }
        for (auto *control : tab_views) {
            if (control && visible(*control) && root_of(control) == owner)
                native::detail::draw_tab_view_at(
                    *control, graphics, origin_in_root(*control));
        }
    }

    void render_collections(native::wnd *owner, native::gpx &graphics) {
        // Parent collection hosts paint before the child collections they
        // contain, matching native child-window stacking.
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

    bool handle_split_mouse(native::wnd *owner,
                            int x,
                            int y,
                            bool pressed,
                            bool released) {
        if (released && active_splitter) {
            native::split_view *control = active_splitter;
            active_splitter = nullptr;
            if (std::find(split_views.begin(), split_views.end(), control) ==
                    split_views.end() ||
                !control->get_created() || root_of(control) != owner)
                return false;
            control->on_native_mouse_click(native::mouse_event(
                native::mouse_button::left,
                native::mouse_action::release,
                local_point(*control, x, y)));
            return true;
        }
        if (!pressed)
            return false;

        for (auto iterator = split_views.rbegin();
             iterator != split_views.rend(); ++iterator) {
            native::split_view *control = *iterator;
            if (!control || !visible(*control) ||
                root_of(control) != owner)
                continue;
            const native::point local = local_point(*control, x, y);
            if (!control->get_splitter_bounds().contains(local))
                continue;
            active_splitter = control;
            control->on_native_mouse_click(native::mouse_event(
                native::mouse_button::left,
                native::mouse_action::press,
                local));
            return true;
        }
        return false;
    }

    bool handle_split_motion(native::wnd *owner, int x, int y) {
        native::split_view *control = active_splitter;
        if (!control ||
            std::find(split_views.begin(), split_views.end(), control) ==
                split_views.end() ||
            !control->get_created() ||
            root_of(control) != owner)
            return false;
        control->on_native_mouse_move(local_point(*control, x, y));
        return true;
    }

    bool handle_collection_mouse(native::wnd *owner,
                                 int x,
                                 int y,
                                 bool pressed,
                                 bool released,
                                 int clicks) {
        if (released && finish_scrollbar_pointer(owner, x, y))
            return true;
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
            control->on_native_mouse_click(native::mouse_event(
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
                auto *state = table_view_bindings.object_from_handle(control);
                if (state && native::detail::begin_table_scrollbar_drag(
                        *control,
                        local_point(*control, x, y),
                        state->scrollbar_horizontal,
                        state->scrollbar_grab_offset)) {
                    state->scrollbar_pointer_active = true;
                    state->scrollbar_dragging = true;
                    return true;
                }
            }
            const native::point local = local_point(*control, x, y);
            control->on_native_mouse_click(native::mouse_event(
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
                auto *state = icon_view_bindings.object_from_handle(control);
                const native::point local = local_point(*control, x, y);
                if (state && begin_scrollbar_pointer(
                        *control,
                        *state,
                        icon_scrollbar(*control),
                        local)) {
                    return true;
                }
            }
            const native::point local = local_point(*control, x, y);
            control->on_native_mouse_click(native::mouse_event(
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
                auto *state = tree_view_bindings.object_from_handle(control);
                const native::point local = local_point(*control, x, y);
                if (state && begin_scrollbar_pointer(
                        *control,
                        *state,
                        tree_scrollbar(*control),
                        local)) {
                    return true;
                }
            }
            const native::point local = local_point(*control, x, y);
            control->on_native_mouse_click(native::mouse_event(
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
            control->on_native_mouse_click(native::mouse_event(
                native::mouse_button::left,
                pressed ? native::mouse_action::press
                        : native::mouse_action::release,
                local_point(*control, x, y)));
            return true;
        }
        for (auto iterator = tab_views.rbegin();
             iterator != tab_views.rend(); ++iterator) {
            native::tab_view *control = *iterator;
            if (!control || !visible(*control) ||
                root_of(control) != owner || !hit(*control, x, y))
                continue;
            control->on_native_mouse_click(native::mouse_event(
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
                control->on_native_mouse_wheel(
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
        for (auto *control : table_views) {
            auto *state = control
                ? table_view_bindings.object_from_handle(control)
                : nullptr;
            if (state && state->scrollbar_dragging &&
                root_of(control) == owner) {
                return native::detail::drag_table_scrollbar(
                    *control,
                    local_point(*control, x, y),
                    state->scrollbar_horizontal,
                    state->scrollbar_grab_offset);
            }
        }
        for (auto *control : icon_views) {
            auto *state = control
                ? icon_view_bindings.object_from_handle(control)
                : nullptr;
            if (state && state->scrollbar_dragging &&
                root_of(control) == owner) {
                return drag_scrollbar_pointer(
                    *control,
                    *state,
                    icon_scrollbar(*control),
                    local_point(*control, x, y));
            }
        }
        for (auto *control : tree_views) {
            auto *state = control
                ? tree_view_bindings.object_from_handle(control)
                : nullptr;
            if (state && state->scrollbar_dragging &&
                root_of(control) == owner) {
                return drag_scrollbar_pointer(
                    *control,
                    *state,
                    tree_scrollbar(*control),
                    local_point(*control, x, y));
            }
        }
        for (auto iterator = code_edits.rbegin();
             iterator != code_edits.rend(); ++iterator) {
            native::code_edit *control = *iterator;
            if (control && visible(*control) &&
                root_of(control) == owner && hit(*control, x, y)) {
                control->on_native_mouse_move(
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
    void tab_view::apply_items() { invalidate(); }
    void tab_view::apply_selected_index() { invalidate(); }

    void tab_view::create_native() {
        if (!get_parent() || !get_parent()->get_created())
            throw std::runtime_error(
                "SDL2: tab_view requires a created parent.");
        auto *self = this;
        linux::sdl2::tab_view_bindings.register_pair(
            self, new linux::sdl2::sdl2_collection());
        linux::sdl2::tab_views.push_back(self);
        self->synchronize_theme_metrics();
        self->refresh();
    }

    void tab_view::show_native() {
        auto *state = linux::sdl2::tab_view_bindings.object_from_handle(
            this);
        if (!_created || !state)
            throw std::runtime_error("SDL2: tab_view is not created.");
        state->visible = true;
        invalidate();
    }

    void tab_view::destroy_native() {
        if (!_created) return;
        auto *self = this;
        auto *state = linux::sdl2::tab_view_bindings.object_from_handle(self);
        linux::sdl2::tab_views.erase(
            std::remove(linux::sdl2::tab_views.begin(),
                        linux::sdl2::tab_views.end(), self),
            linux::sdl2::tab_views.end());
        linux::sdl2::tab_view_bindings.unregister_by_handle(self);
        delete state;
    }

    void accordion::apply_items() { invalidate(); }

    void accordion::create_native() {
        if (!get_parent() || !get_parent()->get_created())
            throw std::runtime_error(
                "SDL2: accordion requires a created parent.");
        auto *self = this;
        linux::sdl2::accordion_bindings.register_pair(
            self, new linux::sdl2::sdl2_collection());
        linux::sdl2::accordions.push_back(self);
        self->synchronize_theme_metrics();
        self->refresh();
    }

    void accordion::show_native() {
        auto *state = linux::sdl2::accordion_bindings
                          .object_from_handle(
                              this);
        if (!_created || !state)
            throw std::runtime_error("SDL2: accordion is not created.");
        state->visible = true;
        invalidate();
    }

    void accordion::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *state = linux::sdl2::accordion_bindings
                          .object_from_handle(self);
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

    void icon_view::create_native() {
        if (!get_parent() || !get_parent()->get_created())
            throw std::runtime_error(
                "SDL2: icon_view requires a created parent.");
        auto *self = this;
        linux::sdl2::icon_view_bindings.register_pair(
            self, new linux::sdl2::sdl2_collection());
        linux::sdl2::icon_views.push_back(self);
        self->synchronize_theme_metrics();
    }

    void icon_view::show_native() {
        auto *state = linux::sdl2::icon_view_bindings
                          .object_from_handle(
                              this);
        if (!_created || !state)
            throw std::runtime_error("SDL2: icon_view is not created.");
        state->visible = true;
        invalidate();
    }

    void icon_view::destroy_native() {
        if (!_created)
            return;
        auto *self = this;
        auto *state = linux::sdl2::icon_view_bindings
                          .object_from_handle(self);
        linux::sdl2::icon_views.erase(
            std::remove(linux::sdl2::icon_views.begin(),
                        linux::sdl2::icon_views.end(),
                        self),
            linux::sdl2::icon_views.end());
        linux::sdl2::icon_view_bindings.unregister_by_handle(self);
        delete state;
    }

} // namespace native
