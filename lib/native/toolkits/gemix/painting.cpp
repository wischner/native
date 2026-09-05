//
// Implements the GEMix coalesced visible-region painting backend.
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

    // GEM dialog edge, outside to inside: black, white, black, black.
    // It belongs to the host, never to its child controls or paint callback.
    void draw_dialog_frame(native::app_wnd *owner,
                           const native::rect &visible) {
        if (!owner->get_modal()) return;
        const auto outer = linux::gemix::outer_rect(
            linux::gemix::wnd_bindings.handle_from_object(owner));
        native::gpx_wnd g(owner, outer.p);
        g.set_clip(native::rect(visible.x1() - outer.x1(),
            visible.y1() - outer.y1(), visible.w(), visible.h()));
        for (int inset = 0; inset < linux::gemix::dialog_frame_width; ++inset) {
            const int value = inset == 1 ? 255 : 0;
            g.set_ink(native::rgba(value, value, value, 255));
            g.draw_rect(native::rect(inset, inset,
                std::max(0, int(outer.w()) - 2 * inset),
                std::max(0, int(outer.h()) - 2 * inset)), false);
        }
    }

    void draw_controls(native::app_wnd *owner, native::gpx &graphics) {
        linux::gemix::render_surfaces(owner, graphics);
        linux::gemix::render_accordions(owner, graphics);
        linux::gemix::render_tab_views(owner, graphics);
        for (auto *button : linux::gemix::buttons) {
            auto saved = graphics.save_state();
            if (!button || root_of(button) != owner)
                continue;

            auto painter = native::theme::create(graphics);
            native::theme::state button_state;
            button_state.pressed = linux::gemix::runtime.pressed_button == button;
            native::detail::control_render_access::draw(
                *button,
                graphics,
                *painter,
                root_bounds(*button),
                button_state);
        }

        auto painter = native::theme::create(graphics);
        for (auto *control : linux::gemix::checks) {
            auto saved = graphics.save_state();
            if (!control || root_of(control) != owner)
                continue;
            native::theme::state state;
            native::detail::control_render_access::draw(
                *control,
                graphics,
                *painter,
                root_bounds(*control),
                state);
        }
        for (auto *control : linux::gemix::radios) {
            auto saved = graphics.save_state();
            if (!control || root_of(control) != owner)
                continue;
            native::theme::state state;
            native::detail::control_render_access::draw(
                *control,
                graphics,
                *painter,
                root_bounds(*control),
                state);
        }
        for (auto *control : linux::gemix::lists) {
            auto saved = graphics.save_state();
            if (!control || root_of(control) != owner)
                continue;
            // A framed tab page already supplies the viewport enclosure.
            if (auto *tabs = dynamic_cast<native::tab_view *>(control->get_parent());
                tabs && tabs->get_page_frame_visible()) {
                const auto bounds = root_bounds(*control);
                graphics.set_clip(graphics.get_clip().intersect(native::rect(
                    bounds.x1() + 1, bounds.y1() + 1,
                    std::max(0, int(bounds.w()) - 2),
                    std::max(0, int(bounds.h()) - 2))));
            }
            native::detail::control_render_access::draw(
                *control,
                graphics,
                *painter,
                root_bounds(*control),
                native::theme::state{});
        }
        for (auto *control : linux::gemix::combo_boxes) {
            auto saved = graphics.save_state();
            if (!control || root_of(control) != owner) continue;
            const native::rect combo_bounds = root_bounds(*control);
            native::detail::control_render_access::draw(
                *control, graphics, *painter, combo_bounds,
                native::theme::state{});
        }
        linux::gemix::render_text_edits(owner, graphics);
        linux::gemix::render_collections(owner, graphics);
        // Popups are overlays: they must follow every ordinary child.
        for (auto *control : linux::gemix::combo_boxes) {
            auto saved = graphics.save_state();
            if (!control || root_of(control) != owner) continue;
            const native::rect combo_bounds = root_bounds(*control);
            auto *state = linux::gemix::combo_box_bindings
                              .object_from_handle(control);
            if (!state || !state->open) continue;
            const int row_height = painter->defaults().list_item_height;
            const native::rect box(combo_bounds.x1(),
                combo_bounds.y2(), combo_bounds.w(),
                static_cast<native::dim>(
                    control->get_items().size()*row_height + 2));
            painter->draw_popup_frame(box);
            for (std::size_t index = 0;
                 index < control->get_items().size(); ++index) {
                native::theme::state item_state;
                item_state.selected = static_cast<int>(index) ==
                                      control->get_selected_index();
                painter->draw_list_item(
                    native::rect(box.x1() + 1,
                        static_cast<native::coord>(box.y1() + 1 +
                            index*row_height), std::max(0, int(box.w()) - 2),
                        static_cast<native::dim>(row_height)),
                    control->get_items()[index], item_state);
            }
        }
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
            draw_dialog_frame(owner, piece);
            piece = piece.intersect(work);
            if (clip)
                piece = piece.intersect(*clip);

            if (piece.w() > 0 && piece.h() > 0) {
                linux::gemix::runtime.painting = true;
                linux::gemix::runtime.paint_clip = piece;
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
                owner->on_native_paint(e);
                g.set_clip(local_piece);
                draw_controls(owner, g);
                linux::gemix::runtime.painting = false;
            }

            wind_get(handle,
                     WF_NEXTXYWH,
                     &box.g_x,
                     &box.g_y,
                     &box.g_w,
                     &box.g_h);
        }

        wind_update(END_UPDATE);
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

        auto *state = window_states.object_from_handle(owner);
        if (!state) return;
        native::rect dirty = area ? *area : native::rect(
            0, 0, target->get_dimensions().w, target->get_dimensions().h);
        const native::point origin = target == root
            ? native::point() : origin_in_root(*target);
        dirty.p.x += origin.x;
        dirty.p.y += origin.y;
        if (state->pending) {
            const int x = std::min<int>(dirty.x1(), state->dirty.x1());
            const int y = std::min<int>(dirty.y1(), state->dirty.y1());
            dirty = native::rect(x, y,
                std::max<int>(dirty.x2(), state->dirty.x2()) - x,
                std::max<int>(dirty.y2(), state->dirty.y2()) - y);
        }
        state->dirty = dirty;
        state->pending = true;
    }

    void flush_repaints() {
        for (auto *owner : windows) {
            auto *state = window_states.object_from_handle(owner);
            if (!state || !state->pending || !owner->get_visible()) continue;
            native::rect clip = state->dirty;
            state->pending = false;
            const auto work = work_rect(wnd_bindings.handle_from_object(owner));
            clip.p.x += work.p.x;
            clip.p.y += work.p.y;
            paint_window(owner, &clip);
        }
    }
} // namespace linux::gemix
