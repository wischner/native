#include <algorithm>
#include <stdexcept>

#include <gem.h>

#include <native.h>

#include "globals.h"
#include "gpx_wnd.h"

namespace
{
    native::rect work_rect_for_handle(WORD handle)
    {
        WORD x = 0;
        WORD y = 0;
        WORD w = 0;
        WORD h = 0;
        wind_get(handle, WF_WORKXYWH, &x, &y, &w, &h);
        return native::rect(x, y, static_cast<native::dim>(w), static_cast<native::dim>(h));
    }

    void draw_buttons(native::app_wnd *owner, const native::rect &work)
    {
        for (auto *button : gemix::buttons)
        {
            if (!button || !button->parent() || button->parent() != owner)
                continue;

            native::gpx_wnd g(owner, native::point(work.p.x, work.p.y));
            g.set_clip(button->bounds());
            native::control_paint painter(g);
            painter.draw_button(button->bounds(), button->text());
        }
    }

    void paint_window(native::app_wnd *owner, const native::rect *clip)
    {
        WORD handle = gemix::wnd_bindings.from_b(owner);
        if (handle <= 0)
            return;

        native::rect work = work_rect_for_handle(handle);
        GRECT box{};
        wind_get(handle, WF_FIRSTXYWH, &box.g_x, &box.g_y, &box.g_w, &box.g_h);

        wind_update(BEG_UPDATE);

        while (box.g_w > 0 && box.g_h > 0)
        {
            native::rect piece(box.g_x, box.g_y, box.g_w, box.g_h);
            if (clip)
                piece = piece.intersect(*clip);

            if (piece.w() > 0 && piece.h() > 0)
            {
                native::gpx_wnd g(owner, native::point(work.p.x, work.p.y));
                g.set_clip(native::rect(0, 0, work.d.w, work.d.h));
                g.set_paper(native::rgba(0, 0, 0, 255));
                g.set_ink(native::rgba(255, 255, 255, 255));
                g.clear(g.paper());
                native::wnd_paint_event e(native::rect(0, 0, work.d.w, work.d.h), g);
                owner->on_wnd_paint.emit(e);
                draw_buttons(owner, work);
                v_updwk(gemix::runtime.vdi_handle);
            }

            wind_get(handle, WF_NEXTXYWH, &box.g_x, &box.g_y, &box.g_w, &box.g_h);
        }

        wind_update(END_UPDATE);
    }

    native::button *button_at(native::app_wnd *owner, native::point p)
    {
        for (auto *button : gemix::buttons)
        {
            if (!button || button->parent() != owner)
                continue;
            if (button->bounds().contains(p))
                return button;
        }
        return nullptr;
    }
}

namespace gemix
{
    void request_repaint(native::wnd *target)
    {
        native::wnd *root = target;

        while (root && root->parent())
            root = root->parent();

        auto *owner = dynamic_cast<native::app_wnd *>(root);
        if (!owner)
            return;

        paint_window(owner, nullptr);
    }
}

namespace native
{
    int app::main_loop()
    {
        if (!gemix::ensure_runtime())
            throw std::runtime_error("GEMix: runtime is not available for main loop.");

        auto *main = app::main_wnd();
        if (!main)
            return -1;

        WORD handle = gemix::wnd_bindings.from_b(main);
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

        // Force the first frame so apps become visible even if the hosted
        // window manager does not deliver an initial redraw immediately.
        paint_window(main, nullptr);

        while (!gemix::runtime.shutdown_requested)
        {
            WORD events = evnt_multi(MU_MESAG | MU_KEYBD | MU_TIMER,
                                     1, 1, 1,
                                     0, 0, 0, 0, 0,
                                     0, 0, 0, 0, 0,
                                     msg,
                                     2, 0,
                                     &mx, &my, &mb, &ks, &kr, &br);

            rect work = work_rect_for_handle(handle);
            point local(mx - work.p.x, my - work.p.y);

            if (mx != prev_mx || my != prev_my)
                main->on_mouse_move.emit(local);

            if ((prev_mb & 1) == 0 && (mb & 1) != 0)
                main->on_mouse_click.emit(mouse_event(mouse_button::left, mouse_action::press, local));

            if ((prev_mb & 1) != 0 && (mb & 1) == 0)
            {
                main->on_mouse_click.emit(mouse_event(mouse_button::left, mouse_action::release, local));
                if (auto *button = button_at(main, local))
                    button->on_click.emit();
            }

            prev_mx = mx;
            prev_my = my;
            prev_mb = mb;

            if ((events & MU_KEYBD) != 0)
            {
                if ((kr & 0xff) == 27)
                    main->destroy();
            }

            if ((events & MU_MESAG) != 0)
            {
                switch (msg[0])
                {
                case WM_REDRAW:
                {
                    rect clip(msg[4], msg[5], msg[6], msg[7]);
                    if (msg[3] == handle)
                        paint_window(main, &clip);
                    break;
                }

                case WM_CLOSED:
                    if (msg[3] == handle)
                        main->destroy();
                    break;

                case WM_MOVED:
                case WM_SIZED:
                    if (msg[3] == handle)
                    {
                        wind_set(handle, WF_CURRXYWH, msg[4], msg[5], msg[6], msg[7]);
                        main->on_native_resize(size(msg[6], msg[7]));
                        main->on_wnd_resize.emit(size(msg[6], msg[7]));
                        main->on_wnd_move.emit(point(msg[4], msg[5]));
                    }
                    break;

                case WM_TOPPED:
                    if (msg[3] == handle)
                        wind_set(handle, WF_TOP, 0, 0, 0, 0);
                    break;

                case WM_FULLED:
                {
                    rect desktop = gemix::desktop_rect();
                    wind_set(handle, WF_CURRXYWH,
                             desktop.p.x, desktop.p.y, desktop.d.w, desktop.d.h);
                    main->on_native_resize(desktop.d);
                    main->on_wnd_resize.emit(desktop.d);
                    break;
                }

                case MN_SELECTED:
                {
                    if (OBJECT *tree = gemix::menu_tree_for(main))
                    {
                        const int item_id = gemix::menu_item_id_for(main, msg[4]);
                        if (item_id != 0)
                            main->on_menu.emit(item_id);
                        menu_tnormal(tree, msg[3], 1);
                    }
                    break;
                }

                default:
                    break;
                }
            }
        }

        gemix::shutdown_runtime();
        return 0;
    }
}
