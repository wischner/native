//
// Implements the GEMix application event-loop backend.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <algorithm>
#include <stdexcept>

#include <gem.h>

#include <native.h>
#include <native/app.h>

#include "globals.h"
#include "gpx_wnd.h"
#include "../../control_render_access.h"
#include "../../post_backend.h"

namespace
{
    using linux::gemix::root_bounds;
    using linux::gemix::root_of;

    native::button *button_at(native::app_wnd *owner, native::point p) {
        for (auto *button : linux::gemix::buttons) {
            if (!button || root_of(button) != owner)
                continue;
            if (root_bounds(*button).contains(p))
                return button;
        }
        return nullptr;
    }

    void activate_selection_control(native::app_wnd *owner,
                                    native::point p) {
        for (auto *control : linux::gemix::combo_boxes) {
            auto *state = control
                ? linux::gemix::combo_box_bindings.object_from_handle(control)
                : nullptr;
            if (!control || !state || root_of(control) != owner)
                continue;
            const native::rect combo_bounds = root_bounds(*control);
            const int row_height = 20;
            const native::rect popup(combo_bounds.x1() + 1,
                combo_bounds.y2() + 1, std::max(0, int(combo_bounds.w()) - 2),
                static_cast<native::dim>(
                    control->get_items().size()*row_height));
            if (state->open && popup.contains(p)) {
                control->on_native_selection(
                    (p.y-popup.y1())/row_height);
                state->open = false;
                state->focused = true;
                control->on_native_drop_down(false);
                owner->invalidate();
                return;
            }
            if (combo_bounds.contains(p)) {
                state->open = !state->open;
                state->focused = true;
                control->on_native_drop_down(state->open);
                owner->invalidate();
                return;
            }
            state->open = false;
            state->focused = false;
        }
        for (auto *control : linux::gemix::checks) {
            if (control && root_of(control) == owner &&
                root_bounds(*control).contains(p)) {
                control->on_native_checked(!control->get_checked());
                return;
            }
        }
        for (auto *control : linux::gemix::radios) {
            if (control && root_of(control) == owner &&
                root_bounds(*control).contains(p)) {
                control->on_native_selected();
                return;
            }
        }
        for (auto *control : linux::gemix::lists) {
            const native::rect bounds = control
                ? root_bounds(*control) : native::rect();
            if (!control || root_of(control) != owner ||
                !bounds.contains(p))
                continue;
            const int index =
                (p.y - bounds.p.y - 1) / 20;
            if (index >= 0 &&
                index < static_cast<int>(control->get_items().size())) {
                control->invalidate();
                control->on_native_selection(index);
            }
            return;
        }
    }

    native::modal_wnd *active_modal_for(native::app_wnd *window) {
        for (native::app_wnd *current = window; current;
             current = current->get_owner()) {
            if (native::modal_wnd *active =
                    current->get_active_modal()) {
                return active;
            }
        }
        return nullptr;
    }

    void raise_active_modal(native::app_wnd *window) {
        native::modal_wnd *active = active_modal_for(window);
        WORD handle =
            active ? linux::gemix::wnd_bindings.handle_from_object(
                         active)
                   : 0;
        if (handle > 0)
            wind_set(handle, WF_TOP, 0, 0, 0, 0);
    }

    native::app_wnd *window_from_handle(WORD handle) {
        return dynamic_cast<native::app_wnd *>(
            linux::gemix::wnd_bindings.object_from_handle(handle));
    }
} // namespace

namespace native
{
    int app::main_loop() {
        if (!linux::gemix::ensure_runtime())
            throw std::runtime_error(
                "GEMix: runtime is not available for main loop.");

        auto *main = app::main_wnd();
        if (!main)
            return -1;

        WORD msg[8] = {};
        WORD mx = 0;
        WORD my = 0;
        WORD mb = 0;
        WORD ks = 0;
        WORD kr = 0;
        WORD br = 0;
        WORD prev_mx = 0;
        WORD prev_my = 0;
        WORD prev_mb = 0;
        graf_mkstate(&prev_mx, &prev_my, &prev_mb, &ks);

        // Force first frames so all windows become visible even if the
        // hosted manager omits an initial redraw.
        for (app_wnd *window : linux::gemix::windows) {
            if (window && window->get_created())
                linux::gemix::request_repaint(window);
        }
        linux::gemix::flush_repaints();

        detail::drain_posted_work();
        while (!linux::gemix::runtime.shutdown_requested) {
            WORD events = evnt_multi(MU_MESAG | MU_KEYBD | MU_BUTTON |
                                         MU_TIMER,
                                     1,
                                     1,
                                     (prev_mb & 1) == 0 ? 1 : 0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     0,
                                     msg,
                                     16,
                                     0,
                                     &mx,
                                     &my,
                                     &mb,
                                     &ks,
                                     &kr,
                                     &br);

            WORD pointer_handle = wind_find(mx, my);
            if ((prev_mb & 1) != 0) {
                for (auto *owner : linux::gemix::windows) {
                    auto *state = linux::gemix::window_states.object_from_handle(owner);
                    if (state && state->capture) {
                        pointer_handle = linux::gemix::wnd_bindings.handle_from_object(owner);
                        break;
                    }
                }
                if (auto *pressed = linux::gemix::runtime.pressed_button)
                    pointer_handle = linux::gemix::wnd_bindings.handle_from_object(root_of(pressed));
            }
            app_wnd *pointer_window =
                window_from_handle(pointer_handle);
            if (pointer_window &&
                pointer_window->get_input_enabled()) {
                const rect work =
                    linux::gemix::work_rect(pointer_handle);
                const point local(mx - work.p.x, my - work.p.y);

                if (mx != prev_mx || my != prev_my) {
                    linux::gemix::update_mouse_cursor(
                        pointer_window, local);
                    linux::gemix::update_collection_pointer(
                        pointer_window, local);
                    if (!linux::gemix::dispatch_drag_move(pointer_window, local) &&
                        !linux::gemix::dispatch_surface_move(
                            pointer_window, local)) {
                        pointer_window->on_native_mouse_move(
                            local, point(mx, my));
                    }
                }

                if ((prev_mb & 1) == 0 && (mb & 1) != 0) {
                    linux::gemix::runtime.pressed_button = button_at(pointer_window, local);
                    if (auto *pressed = linux::gemix::runtime.pressed_button)
                        pressed->invalidate();
                    linux::gemix::active_window = pointer_window;
                    linux::gemix::focus_text_edit(pointer_window,
                                                  local);
                    if (!linux::gemix::dispatch_drag_click(pointer_window, local, true) &&
                        !linux::gemix::dispatch_surface_click(
                            pointer_window, local, true)) {
                        pointer_window->on_native_mouse_click(
                            mouse_event(mouse_button::left,
                                        mouse_action::press,
                                        local));
                    }
                }

                if ((prev_mb & 1) != 0 && (mb & 1) == 0) {
                    auto *pressed = linux::gemix::runtime.pressed_button;
                    linux::gemix::runtime.pressed_button = nullptr;
                    if (pressed) {
                        pressed->invalidate();
                        linux::gemix::flush_repaints();
                    }
                    if (linux::gemix::dispatch_drag_click(pointer_window, local, false) ||
                        linux::gemix::dispatch_surface_click(
                            pointer_window, local, false)) {
                        // A canvas or panel region owns this
                        // position, so no control activation follows.
                    } else {
                        pointer_window->on_native_mouse_click(
                            mouse_event(mouse_button::left,
                                        mouse_action::release,
                                        local));
                        if (auto *button =
                                button_at(pointer_window, local)) {
                            if (button == pressed)
                                button->on_native_click();
                        } else if (!pressed && !linux::gemix::activate_collection(
                                       pointer_window, local)) {
                            activate_selection_control(pointer_window,
                                                       local);
                        }
                    }
                }
            } else if (pointer_window &&
                       ((prev_mb & 1) == 0 && (mb & 1) != 0)) {
                raise_active_modal(pointer_window);
            }

            if (!pointer_window &&
                (mx != prev_mx || my != prev_my)) {
                linux::gemix::update_mouse_cursor(nullptr, point());
            }

            prev_mx = mx;
            prev_my = my;
            prev_mb = mb;

            if ((events & MU_KEYBD) != 0) {
                app_wnd *top = linux::gemix::active_window
                                   ? linux::gemix::active_window
                                   : app::main_wnd();
                if (top && !top->get_input_enabled()) {
                    raise_active_modal(top);
                } else if (top && linux::gemix::handle_text_edit_key(
                                      top, ks, kr)) {
                    // A focused editor owns its selection/clipboard commands.
                } else if (top && linux::gemix::handle_combo_key(
                                      top, ks, kr)) {
                    // The focused combo box consumed this key packet.
                } else if (top && linux::gemix::handle_collection_key(
                                      top, ks, kr)) {
                    // A collection control consumed this key packet.
                } else if (top && linux::gemix::handle_menu_key(
                                      top, ks, kr)) {
                    // Unhandled accelerators remain available to the menu.
                } else if (top && (kr & 0xff) == 27) {
                    top->destroy();
                }
            }

            if ((events & MU_MESAG) != 0) {
                app_wnd *target = window_from_handle(msg[3]);
                switch (msg[0]) {
                case WM_REDRAW: {
                    rect clip(msg[4], msg[5], msg[6], msg[7]);
                    if (target)
                    {
                        const auto work = linux::gemix::work_rect(msg[3]);
                        clip.p.x -= work.p.x;
                        clip.p.y -= work.p.y;
                        linux::gemix::request_repaint(target, &clip);
                    }
                    break;
                }

                case WM_CLOSED:
                    if (target && target->get_input_enabled())
                        target->destroy();
                    else if (target)
                        raise_active_modal(target);
                    break;

                case WM_MOVED:
                case WM_SIZED:
                    if (target && target->get_input_enabled()) {
                        wind_set(msg[3],
                                 WF_CURRXYWH,
                                 msg[4],
                                 msg[5],
                                 msg[6],
                                 msg[7]);

                        // The message carries the outer rectangle.
                        // Ask AES what work area that leaves, so the
                        // client size the portable layer arranges
                        // children in excludes the title, borders,
                        // and any sliders the window carries.
                        const size work =
                            linux::gemix::work_rect(msg[3]).d;
                        target->on_native_move(
                            point(msg[4], msg[5]));
                        target->on_native_resize(work);
                    } else if (target) {
                        raise_active_modal(target);
                    }
                    break;

                case WM_TOPPED:
                    if (target && target->get_input_enabled()) {
                        wind_set(msg[3], WF_TOP, 0, 0, 0, 0);
                        linux::gemix::active_window = target;
                    } else if (target) {
                        raise_active_modal(target);
                    }
                    break;

                case WM_FULLED: {
                    if (!target || !target->get_input_enabled()) {
                        if (target)
                            raise_active_modal(target);
                        break;
                    }

                    rect desktop = linux::gemix::desktop_rect();
                    wind_set(msg[3],
                             WF_CURRXYWH,
                             desktop.p.x,
                             desktop.p.y,
                             desktop.d.w,
                             desktop.d.h);

                    const size work = linux::gemix::work_rect(msg[3]).d;
                    target->on_native_move(desktop.p);
                    target->on_native_resize(work);
                    break;
                }

                case MN_SELECTED: {
                    app_wnd *menu_owner = app::main_wnd();
                    if (menu_owner &&
                        !menu_owner->get_input_enabled()) {
                        raise_active_modal(menu_owner);
                        break;
                    }
                    if (OBJECT *tree = linux::gemix::menu_tree_for(
                            menu_owner)) {
                        const int item_id =
                            linux::gemix::menu_item_id_for(
                                menu_owner, msg[4]);
                        menu_tnormal(tree, msg[3], 1);
                        if (item_id != 0)
                            menu_owner->on_native_menu(item_id);
                    }
                    break;
                }

                default:
                    break;
                }
            }
            detail::drain_posted_work();
            linux::gemix::flush_repaints();
        }

        linux::gemix::shutdown_runtime();
        return 0;
    }
} // namespace native
