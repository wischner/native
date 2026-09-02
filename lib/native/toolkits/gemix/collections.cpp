//
// Implements GEM collection and source-editor painting and dispatch.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <string>

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

    native::point local_point(native::wnd &control,
                              native::point point) {
        const native::point origin = origin_in_root(control);
        return native::point(point.x - origin.x, point.y - origin.y);
    }

    bool hit(native::wnd &control, native::point point) {
        return native::rect(origin_in_root(control),
                            control.get_dimensions())
            .contains(point);
    }

    void clear_focus(native::app_wnd *root) {
        for (auto *control : linux::gemix::accordions) {
            if (control && root_of(control) == root)
                control->on_native_focus(false);
        }
        for (auto *control : linux::gemix::icon_views) {
            if (control && root_of(control) == root)
                control->on_native_focus(false);
        }
        for (auto *control : linux::gemix::tree_views) {
            if (control && root_of(control) == root)
                control->on_native_focus(false);
        }
        for (auto *control : linux::gemix::table_views) {
            if (control && root_of(control) == root)
                control->on_native_focus(false);
        }
        for (auto *control : linux::gemix::code_edits) {
            if (control && root_of(control) == root)
                control->on_native_focus(false);
        }
    }

    native::icon_view *last_clicked = nullptr;
    int last_clicked_item = -1;
    std::chrono::steady_clock::time_point last_click_time;
    native::tree_view *last_clicked_tree = nullptr;
    native::tree_item_id last_clicked_tree_item =
        native::invalid_tree_item_id;
    std::chrono::steady_clock::time_point last_tree_click_time;
    native::table_view *last_clicked_table = nullptr;
    native::table_row_id last_clicked_row =
        native::invalid_table_row_id;
    std::chrono::steady_clock::time_point last_table_click_time;
} // namespace

namespace linux::gemix
{
    void forget_tree_click(native::tree_view *control) {
        if (last_clicked_tree != control)
            return;
        last_clicked_tree = nullptr;
        last_clicked_tree_item = native::invalid_tree_item_id;
    }

    void render_tab_views(native::app_wnd *parent,
                          native::gpx &graphics) {
        for (auto *control : tab_views) {
            if (control && control->get_created() &&
                root_of(control) == parent) {
                native::detail::draw_tab_view_at(
                    *control, graphics, origin_in_root(*control));
            }
        }
    }

    void render_collections(native::app_wnd *parent,
                            native::gpx &graphics) {
        for (auto *control : icon_views) {
            if (control && control->get_created() &&
                root_of(control) == parent) {
                native::detail::draw_icon_view_at(
                    *control, graphics, origin_in_root(*control));
            }
        }
        for (auto *control : tree_views) {
            if (control && control->get_created() &&
                root_of(control) == parent) {
                native::detail::draw_tree_view_at(
                    *control, graphics, origin_in_root(*control));
            }
        }
        for (auto *control : table_views) {
            if (control && control->get_created() &&
                root_of(control) == parent) {
                native::detail::draw_table_view_at(
                    *control, graphics, origin_in_root(*control));
            }
        }
        for (auto *control : code_edits) {
            if (control && control->get_created() &&
                root_of(control) == parent) {
                native::draw_code_edit(
                    *control, graphics, origin_in_root(*control));
            }
        }
        // Match the native child-window stacking order. Composite controls create
        // transient empty accordions as its raised guide surfaces.
        for (auto *control : accordions) {
            if (control && control->get_created() &&
                root_of(control) == parent) {
                native::detail::draw_accordion_at(
                    *control, graphics, origin_in_root(*control));
            }
        }
    }

    bool activate_collection(native::app_wnd *parent,
                             native::point point) {
        for (auto iterator = code_edits.rbegin();
             iterator != code_edits.rend(); ++iterator) {
            native::code_edit *control = *iterator;
            if (!control || !control->get_created() ||
                root_of(control) != parent || !hit(*control, point))
                continue;
            clear_focus(parent);
            control->on_native_focus(true);
            control->on_native_mouse_click(native::mouse_event(
                native::mouse_button::left,
                native::mouse_action::press,
                local_point(*control, point)));
            return true;
        }
        for (auto iterator = table_views.rbegin();
             iterator != table_views.rend(); ++iterator) {
            native::table_view *control = *iterator;
            if (!control || !control->get_created() ||
                root_of(control) != parent || !hit(*control, point))
                continue;
            clear_focus(parent);
            control->on_native_focus(true);
            native::detail::handle_table_click(
                *control, local_point(*control, point));
            const auto selected = control->get_selected_rows();
            const native::table_row_id row = selected.empty()
                ? native::invalid_table_row_id
                : selected.back();
            const auto now = std::chrono::steady_clock::now();
            if (row != native::invalid_table_row_id &&
                control == last_clicked_table &&
                row == last_clicked_row &&
                now - last_table_click_time <=
                    std::chrono::milliseconds(500)) {
                control->on_native_activate(row);
                last_clicked_table = nullptr;
                last_clicked_row = native::invalid_table_row_id;
            } else {
                last_clicked_table = control;
                last_clicked_row = row;
                last_table_click_time = now;
            }
            return true;
        }
        for (auto iterator = icon_views.rbegin();
             iterator != icon_views.rend();
             ++iterator) {
            native::icon_view *control = *iterator;
            if (!control || !control->get_created() ||
                root_of(control) != parent || !hit(*control, point))
                continue;
            clear_focus(parent);
            control->on_native_focus(true);
            const native::point local = local_point(*control, point);
            const int item = control->item_at(local);
            control->on_native_selection(item);
            const auto now = std::chrono::steady_clock::now();
            if (item >= 0 && control == last_clicked &&
                item == last_clicked_item &&
                now - last_click_time <= std::chrono::milliseconds(500)) {
                control->on_native_activate(item);
                last_clicked = nullptr;
                last_clicked_item = -1;
            } else {
                last_clicked = control;
                last_clicked_item = item;
                last_click_time = now;
            }
            return true;
        }
        for (auto iterator = tree_views.rbegin();
             iterator != tree_views.rend();
             ++iterator) {
            native::tree_view *control = *iterator;
            if (!control || !control->get_created() ||
                root_of(control) != parent || !hit(*control, point))
                continue;
            clear_focus(parent);
            control->on_native_focus(true);
            const native::point local = local_point(*control, point);
            const native::tree_view_hit tree_hit =
                control->hit_test(local);
            native::detail::handle_tree_view_click(*control, local);
            const auto now = std::chrono::steady_clock::now();
            if (tree_hit.part == native::tree_view_hit_part::row &&
                tree_hit.id != native::invalid_tree_item_id &&
                control == last_clicked_tree &&
                tree_hit.id == last_clicked_tree_item &&
                now - last_tree_click_time <=
                    std::chrono::milliseconds(500)) {
                control->on_native_double_click(tree_hit.id);
                last_clicked_tree = nullptr;
                last_clicked_tree_item =
                    native::invalid_tree_item_id;
            } else {
                last_clicked_tree = control;
                last_clicked_tree_item = tree_hit.id;
                last_tree_click_time = now;
            }
            return true;
        }
        for (auto iterator = accordions.rbegin();
             iterator != accordions.rend();
             ++iterator) {
            native::accordion *control = *iterator;
            if (!control || !control->get_created() ||
                root_of(control) != parent || !hit(*control, point))
                continue;
            clear_focus(parent);
            control->on_native_focus(true);
            native::detail::handle_accordion_click(
                *control, local_point(*control, point));
            return true;
        }
        for (auto iterator = tab_views.rbegin();
             iterator != tab_views.rend(); ++iterator) {
            native::tab_view *control = *iterator;
            if (!control || !control->get_created() ||
                root_of(control) != parent || !hit(*control, point))
                continue;
            control->on_native_mouse_click(native::mouse_event(
                native::mouse_button::left,
                native::mouse_action::release,
                local_point(*control, point)));
            return true;
        }
        return false;
    }

    bool handle_collection_key(native::app_wnd *parent,
                               WORD modifiers,
                               WORD key) {
        const int scan = (key >> 8) & 0xff;
        const int ascii = key & 0xff;
        for (auto *control : code_edits) {
            if (!control || !control->get_created() ||
                root_of(control) != parent || !control->get_focused())
                continue;
            const bool shift = (modifiers & 0x03) != 0;
            const bool command = (modifiers & 0x04) != 0;
            if (command) {
                const int lower = ascii >= 'A' && ascii <= 'Z'
                                      ? ascii - 'A' + 'a'
                                      : ascii;
                if (lower == 'a')
                    return control->on_native_key(
                        native::code_edit_key::select_all);
                if (lower == 'c')
                    return control->on_native_key(
                        native::code_edit_key::copy);
                if (lower == 'x')
                    return control->on_native_key(
                        native::code_edit_key::cut);
                if (lower == 'v')
                    return control->on_native_key(
                        native::code_edit_key::paste);
                if (lower == 'z')
                    return control->on_native_key(
                        shift ? native::code_edit_key::redo
                              : native::code_edit_key::undo);
                return false;
            }
            native::code_edit_key command_key;
            bool handled = true;
            if (scan == 0x4b)
                command_key = native::code_edit_key::left;
            else if (scan == 0x4d)
                command_key = native::code_edit_key::right;
            else if (scan == 0x48)
                command_key = native::code_edit_key::up;
            else if (scan == 0x50)
                command_key = native::code_edit_key::down;
            else if (scan == 0x47)
                command_key = native::code_edit_key::home;
            else if (scan == 0x4f)
                command_key = native::code_edit_key::end;
            else if (scan == 0x49)
                command_key = native::code_edit_key::page_up;
            else if (scan == 0x51)
                command_key = native::code_edit_key::page_down;
            else if (ascii == 8)
                command_key = native::code_edit_key::backspace;
            else if (scan == 0x53)
                command_key = native::code_edit_key::delete_forward;
            else if (ascii == 13)
                command_key = native::code_edit_key::enter;
            else if (ascii == 9)
                command_key = native::code_edit_key::tab;
            else if (ascii == 27)
                command_key = native::code_edit_key::escape;
            else
                handled = false;
            if (handled)
                return control->on_native_key(command_key, shift);
            if (ascii >= 32 && ascii < 127) {
                return control->on_native_text_input(
                    std::string(1, static_cast<char>(ascii)));
            }
            return false;
        }
        for (auto *control : table_views) {
            if (!control || !control->get_created() ||
                root_of(control) != parent || !control->get_focused())
                continue;
            if (scan == 0x48)
                control->on_native_navigation(
                    native::table_navigation::up);
            else if (scan == 0x50)
                control->on_native_navigation(
                    native::table_navigation::down);
            else if (scan == 0x47)
                control->on_native_navigation(
                    native::table_navigation::home);
            else if (scan == 0x4f)
                control->on_native_navigation(
                    native::table_navigation::end);
            else if (scan == 0x49)
                control->on_native_navigation(
                    native::table_navigation::page_up);
            else if (scan == 0x51)
                control->on_native_navigation(
                    native::table_navigation::page_down);
            else if (scan == 0x4b)
                control->on_native_navigation(
                    native::table_navigation::collapse);
            else if (scan == 0x4d)
                control->on_native_navigation(
                    native::table_navigation::expand);
            else if (ascii == 32)
                control->on_native_navigation(
                    native::table_navigation::toggle);
            else if (ascii == 13)
                control->on_native_navigation(
                    native::table_navigation::activate);
            else if (ascii > 32 && ascii < 127)
                control->on_native_type_text(
                    std::string(1, static_cast<char>(ascii)));
            else
                return false;
            return true;
        }
        for (auto *control : accordions) {
            if (!control || !control->get_created() ||
                root_of(control) != parent ||
                control->get_focused_index() < 0)
                continue;
            if (scan == 0x48)
                control->on_native_navigation(
                    native::accordion_navigation::previous);
            else if (scan == 0x50)
                control->on_native_navigation(
                    native::accordion_navigation::next);
            else if (scan == 0x47)
                control->on_native_navigation(
                    native::accordion_navigation::first);
            else if (scan == 0x4f)
                control->on_native_navigation(
                    native::accordion_navigation::last);
            else if (ascii == 13 || ascii == 32)
                control->on_native_navigation(
                    native::accordion_navigation::toggle);
            else
                return false;
            return true;
        }
        for (auto *control : icon_views) {
            if (!control || !control->get_created() ||
                root_of(control) != parent || !control->get_focused())
                continue;
            if (scan == 0x4b)
                control->on_native_navigation(
                    native::icon_view_navigation::left);
            else if (scan == 0x4d)
                control->on_native_navigation(
                    native::icon_view_navigation::right);
            else if (scan == 0x48)
                control->on_native_navigation(
                    native::icon_view_navigation::up);
            else if (scan == 0x50)
                control->on_native_navigation(
                    native::icon_view_navigation::down);
            else if (scan == 0x47)
                control->on_native_navigation(
                    native::icon_view_navigation::home);
            else if (scan == 0x4f)
                control->on_native_navigation(
                    native::icon_view_navigation::end);
            else if (scan == 0x49)
                control->on_native_navigation(
                    native::icon_view_navigation::page_up);
            else if (scan == 0x51)
                control->on_native_navigation(
                    native::icon_view_navigation::page_down);
            else if (ascii == 13)
                control->on_native_activate(
                    control->get_selected_index());
            else
                return false;
            return true;
        }
        for (auto *control : tree_views) {
            if (!control || !control->get_created() ||
                root_of(control) != parent || !control->get_focused())
                continue;
            if (scan == 0x48)
                control->on_native_navigation(
                    native::tree_view_navigation::up);
            else if (scan == 0x50)
                control->on_native_navigation(
                    native::tree_view_navigation::down);
            else if (scan == 0x4b)
                control->on_native_navigation(
                    native::tree_view_navigation::left);
            else if (scan == 0x4d)
                control->on_native_navigation(
                    native::tree_view_navigation::right);
            else if (scan == 0x47)
                control->on_native_navigation(
                    native::tree_view_navigation::home);
            else if (scan == 0x4f)
                control->on_native_navigation(
                    native::tree_view_navigation::end);
            else if (scan == 0x49)
                control->on_native_navigation(
                    native::tree_view_navigation::page_up);
            else if (scan == 0x51)
                control->on_native_navigation(
                    native::tree_view_navigation::page_down);
            else if (ascii == 32)
                control->on_native_navigation(
                    native::tree_view_navigation::toggle);
            else if (ascii == 13)
                control->on_native_navigation(
                    native::tree_view_navigation::activate);
            else
                return false;
            return true;
        }
        return false;
    }

    bool update_collection_pointer(native::app_wnd *parent,
                                   native::point point) {
        for (auto iterator = code_edits.rbegin();
             iterator != code_edits.rend(); ++iterator) {
            native::code_edit *control = *iterator;
            if (control && control->get_created() &&
                root_of(control) == parent && hit(*control, point)) {
                control->on_native_mouse_move(
                    local_point(*control, point));
                return true;
            }
        }
        return false;
    }
} // namespace linux::gemix

namespace native
{
    void tab_view::apply_items() { invalidate(); }
    void tab_view::apply_selected_index() { invalidate(); }

    void tab_view::create() const {
        if (_created) return;
        if (!get_parent() || !get_parent()->get_created())
            throw std::runtime_error(
                "GEMix: tab_view requires a created parent.");
        auto *self = const_cast<tab_view *>(this);
        linux::gemix::tab_views.push_back(self);
        _created = true;
        self->synchronize_theme_metrics();
        self->refresh();
        self->on_native_create();
    }

    void tab_view::show() const {
        if (!_created)
            throw std::runtime_error("GEMix: tab_view is not created.");
        invalidate();
    }

    void tab_view::destroy() const {
        if (!_created) return;
        auto *self = const_cast<tab_view *>(this);
        self->on_native_destroy();
        linux::gemix::tab_views.erase(
            std::remove(linux::gemix::tab_views.begin(),
                        linux::gemix::tab_views.end(), self),
            linux::gemix::tab_views.end());
    }

    void accordion::apply_items() { invalidate(); }

    void accordion::create() const {
        if (_created)
            return;
        if (!get_parent() || !get_parent()->get_created())
            throw std::runtime_error(
                "GEMix: accordion requires a created parent.");
        auto *self = const_cast<accordion *>(this);
        linux::gemix::accordions.push_back(self);
        _created = true;
        self->synchronize_theme_metrics();
        self->refresh();
        self->on_native_create();
    }

    void accordion::show() const {
        if (!_created)
            throw std::runtime_error("GEMix: accordion is not created.");
        invalidate();
    }

    void accordion::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<accordion *>(this);
        self->on_native_destroy();
        linux::gemix::accordions.erase(
            std::remove(linux::gemix::accordions.begin(),
                        linux::gemix::accordions.end(),
                        self),
            linux::gemix::accordions.end());
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
                "GEMix: icon_view requires a created parent.");
        auto *self = const_cast<icon_view *>(this);
        linux::gemix::icon_views.push_back(self);
        _created = true;
        self->synchronize_theme_metrics();
        self->on_native_create();
    }

    void icon_view::show() const {
        if (!_created)
            throw std::runtime_error("GEMix: icon_view is not created.");
        invalidate();
    }

    void icon_view::destroy() const {
        if (!_created)
            return;
        auto *self = const_cast<icon_view *>(this);
        self->on_native_destroy();
        linux::gemix::icon_views.erase(
            std::remove(linux::gemix::icon_views.begin(),
                        linux::gemix::icon_views.end(),
                        self),
            linux::gemix::icon_views.end());
        if (last_clicked == self) {
            last_clicked = nullptr;
            last_clicked_item = -1;
        }
    }

} // namespace native
