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
#include "../../post_backend.h"

namespace
{
    void draw_controls(native::app_wnd *owner, native::gpx &graphics) {
        for (auto *button : linux::gemix::buttons) {
            if (!button || !button->get_parent() ||
                button->get_parent() != owner)
                continue;

            auto painter = native::theme::create(graphics);
            painter->draw_button(button->get_bounds(),
                                 button->get_text());
        }

        auto painter = native::theme::create(graphics);
        for (auto *control : linux::gemix::checks) {
            if (!control || control->get_parent() != owner)
                continue;
            native::theme::state state;
            state.selected = control->get_checked();
            painter->draw_check(
                control->get_bounds(), control->get_text(), state);
        }
        for (auto *control : linux::gemix::radios) {
            if (!control || control->get_parent() != owner)
                continue;
            native::theme::state state;
            state.selected = control->get_selected();
            painter->draw_radio(
                control->get_bounds(), control->get_text(), state);
        }
        for (auto *control : linux::gemix::lists) {
            if (!control || control->get_parent() != owner)
                continue;
            painter->draw_list(control->get_bounds(),
                               control->get_items(),
                               control->get_selected_index());
        }
        linux::gemix::render_text_edits(owner, graphics);
        linux::gemix::render_collections(owner, graphics);
    }

    void paint_window(native::app_wnd *owner,
                      const native::rect *clip) {
        WORD handle =
            linux::gemix::wnd_bindings.handle_from_object(owner);
        if (handle <= 0)
            return;

        native::rect work = linux::gemix::work_rect(handle);
        GRECT box{};
        wind_get(handle,
                 WF_FIRSTXYWH,
                 &box.g_x,
                 &box.g_y,
                 &box.g_w,
                 &box.g_h);

        wind_update(BEG_UPDATE);

        while (box.g_w > 0 && box.g_h > 0) {
            native::rect piece(box.g_x, box.g_y, box.g_w, box.g_h);
            if (clip)
                piece = piece.intersect(*clip);

            if (piece.w() > 0 && piece.h() > 0) {
                const native::rect local_piece(
                    piece.p.x - work.p.x,
                    piece.p.y - work.p.y,
                    piece.d.w,
                    piece.d.h);
                native::gpx_wnd g(owner,
                                  native::point(work.p.x, work.p.y));
                g.set_clip(local_piece);
                g.set_paper(native::rgba(255, 255, 255, 255));
                g.set_ink(native::rgba(0, 0, 0, 255));
                g.clear(g.get_paper());
                native::wnd_paint_event e(local_piece, g);
                owner->on_wnd_paint.emit(e);
                g.set_clip(local_piece);
                draw_controls(owner, g);
            }

            wind_get(handle,
                     WF_NEXTXYWH,
                     &box.g_x,
                     &box.g_y,
                     &box.g_w,
                     &box.g_h);
        }

        v_updwk(linux::gemix::runtime.vdi_handle);
        wind_update(END_UPDATE);
    }

    native::button *button_at(native::app_wnd *owner, native::point p) {
        for (auto *button : linux::gemix::buttons) {
            if (!button || button->get_parent() != owner)
                continue;
            if (button->get_bounds().contains(p))
                return button;
        }
        return nullptr;
    }

    void activate_selection_control(native::app_wnd *owner,
                                    native::point p) {
        for (auto *control : linux::gemix::checks) {
            if (control && control->get_parent() == owner &&
                control->get_bounds().contains(p)) {
                control->on_native_checked(!control->get_checked());
                return;
            }
        }
        for (auto *control : linux::gemix::radios) {
            if (control && control->get_parent() == owner &&
                control->get_bounds().contains(p)) {
                control->on_native_selected();
                return;
            }
        }
        for (auto *control : linux::gemix::lists) {
            if (!control || control->get_parent() != owner ||
                !control->get_bounds().contains(p))
                continue;
            const int index =
                (p.y - control->get_bounds().p.y - 1) / 20;
            if (index >= 0 &&
                index < static_cast<int>(control->get_items().size())) {
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

namespace linux::gemix
{
    void request_repaint(native::wnd *target,
                         const native::rect *area) {
        native::wnd *root = target;

        while (root && root->get_parent())
            root = root->get_parent();

        auto *owner = dynamic_cast<native::app_wnd *>(root);
        if (!owner)
            return;

        if (!area) {
            paint_window(owner, nullptr);
            return;
        }

        const WORD handle = wnd_bindings.handle_from_object(owner);
        const native::rect work = linux::gemix::work_rect(handle);
        const native::rect screen_area(
            area->p.x + work.p.x,
            area->p.y + work.p.y,
            area->d.w,
            area->d.h);
        paint_window(owner, &screen_area);
    }
} // namespace linux::gemix

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
                paint_window(window, nullptr);
        }

        detail::drain_posted_work();
        while (!linux::gemix::runtime.shutdown_requested) {
            WORD events = evnt_multi(MU_MESAG | MU_KEYBD | MU_BUTTON |
                                         MU_TIMER,
                                     1,
                                     1,
                                     1,
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
                                     2,
                                     0,
                                     &mx,
                                     &my,
                                     &mb,
                                     &ks,
                                     &kr,
                                     &br);

            const WORD pointer_handle = wind_find(mx, my);
            app_wnd *pointer_window =
                window_from_handle(pointer_handle);
            if (pointer_window &&
                pointer_window->get_input_enabled()) {
                const rect work =
                    linux::gemix::work_rect(pointer_handle);
                const point local(mx - work.p.x, my - work.p.y);

                if (mx != prev_mx || my != prev_my) {
                    linux::gemix::update_text_edit_cursor(
                        pointer_window, local);
                    linux::gemix::update_collection_pointer(
                        pointer_window, local);
                    pointer_window->on_mouse_move.emit(local);
                }

                if ((prev_mb & 1) == 0 && (mb & 1) != 0) {
                    linux::gemix::active_window = pointer_window;
                    linux::gemix::focus_text_edit(pointer_window,
                                                  local);
                    pointer_window->on_mouse_click.emit(mouse_event(
                        mouse_button::left,
                        mouse_action::press,
                        local));
                }

                if ((prev_mb & 1) != 0 && (mb & 1) == 0) {
                    pointer_window->on_mouse_click.emit(mouse_event(
                        mouse_button::left,
                        mouse_action::release,
                        local));
                    if (auto *button =
                            button_at(pointer_window, local)) {
                        button->on_click.emit();
                    } else if (!linux::gemix::activate_collection(
                                   pointer_window, local)) {
                        activate_selection_control(pointer_window,
                                                   local);
                    }
                }
            } else if (pointer_window &&
                       ((prev_mb & 1) == 0 && (mb & 1) != 0)) {
                raise_active_modal(pointer_window);
            }

            if (!pointer_window &&
                (mx != prev_mx || my != prev_my)) {
                linux::gemix::update_text_edit_cursor(nullptr,
                                                      point());
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
                    // The focused editor consumed this key packet.
                } else if (top && linux::gemix::handle_collection_key(
                                      top, ks, kr)) {
                    // A collection control consumed this key packet.
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
                        paint_window(target, &clip);
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
                        target->on_wnd_resize.emit(work);
                        target->on_wnd_move.emit(
                            point(msg[4], msg[5]));
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
                    target->on_wnd_resize.emit(work);
                    target->on_wnd_move.emit(desktop.p);
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
                        if (item_id != 0)
                            menu_owner->on_menu.emit(item_id);
                        menu_tnormal(tree, msg[3], 1);
                    }
                    break;
                }

                default:
                    break;
                }
            }
        }

        linux::gemix::shutdown_runtime();
        return 0;
    }
} // namespace native
