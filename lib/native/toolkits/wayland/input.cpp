//
// Routes GTK pointer gestures, focus, text input and key commands.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "../../collection_render.h"
#include "../../table_render.h"
#include <cmath>
namespace
{
    using namespace native;
    point position(double x, double y) {
        return point(static_cast<coord>(x), static_cast<coord>(y));
    }
    mouse_button button(guint value) {
        switch (value) {
        case 1:
            return mouse_button::left;
        case 2:
            return mouse_button::middle;
        case 3:
            return mouse_button::right;
        default:
            return mouse_button::none;
        }
    }
    void click(GtkGestureClick *gesture,
               int,
               double x,
               double y,
               gpointer data,
               bool press) {
        auto &owner = *static_cast<wnd *>(data);
        if (!owner.get_input_enabled())
            return;
        if (press)
            gtk_widget_grab_focus(gtk_event_controller_get_widget(
                GTK_EVENT_CONTROLLER(gesture)));
        const point p = position(x, y);
        const auto native_button = gtk_gesture_single_get_current_button(GTK_GESTURE_SINGLE(gesture));
        const bool primary = native_button == 0 || native_button == GDK_BUTTON_PRIMARY;
        if (!press && primary)
            if (auto *table = dynamic_cast<table_view *>(&owner)) {
                auto &peer = linux::wayland::state(owner);
                if (peer.dragging_scrollbar) {
                    detail::drag_table_scrollbar(
                        *table,
                        p,
                        peer.horizontal_scrollbar,
                        peer.scrollbar_grab);
                    peer.dragging_scrollbar = false;
                }
                // Selection/disclosure was handled on press with modifiers.
                // Do not dispatch a second, unmodified portable release.
                return;
            }
        if (press) {
            if (auto *icons = dynamic_cast<icon_view *>(&owner)) {
                icons->on_native_selection(icons->item_at(p));
                return;
            }
            if (auto *table = dynamic_cast<table_view *>(&owner); table && primary) {
                auto &peer = linux::wayland::state(owner);
                peer.dragging_scrollbar =
                    detail::begin_table_scrollbar_drag(
                        *table,
                        p,
                        peer.horizontal_scrollbar,
                        peer.scrollbar_grab);
                if (!peer.dragging_scrollbar) {
                    const auto modifiers = gtk_event_controller_get_current_event_state(
                        GTK_EVENT_CONTROLLER(gesture));
                    detail::handle_table_click(*table, p,
                        modifiers & GDK_CONTROL_MASK, modifiers & GDK_SHIFT_MASK);
                }
                return;
            }
            if (auto *accordion =
                    dynamic_cast<native::accordion *>(&owner)) {
                detail::handle_accordion_click(*accordion, p);
                return;
            }
        }
        owner.on_native_mouse_click(mouse_event(
            button(gtk_gesture_single_get_current_button(
                GTK_GESTURE_SINGLE(gesture))),
            press ? mouse_action::press : mouse_action::release,
            p));
    }
    bool command(wnd &owner, guint key, GdkModifierType modifiers) {
        const bool shift = modifiers & GDK_SHIFT_MASK;
        const bool ctrl = modifiers & GDK_CONTROL_MASK;
        if (auto *table = dynamic_cast<table_view *>(&owner))
            return linux::wayland::table_key(*table, key, modifiers);
        if (auto *editor = dynamic_cast<code_edit *>(&owner)) {
            code_edit_key command;
            if (ctrl) {
                switch (gdk_keyval_to_lower(key)) {
                case GDK_KEY_a:
                    command = code_edit_key::select_all;
                    break;
                case GDK_KEY_c:
                    command = code_edit_key::copy;
                    break;
                case GDK_KEY_x:
                    command = code_edit_key::cut;
                    break;
                case GDK_KEY_v:
                    command = code_edit_key::paste;
                    break;
                case GDK_KEY_z:
                    command = shift ? code_edit_key::redo
                                    : code_edit_key::undo;
                    break;
                default:
                    return false;
                }
            } else
                switch (key) {
                case GDK_KEY_Left:
                    command = code_edit_key::left;
                    break;
                case GDK_KEY_Right:
                    command = code_edit_key::right;
                    break;
                case GDK_KEY_Up:
                    command = code_edit_key::up;
                    break;
                case GDK_KEY_Down:
                    command = code_edit_key::down;
                    break;
                case GDK_KEY_Home:
                    command = code_edit_key::home;
                    break;
                case GDK_KEY_End:
                    command = code_edit_key::end;
                    break;
                case GDK_KEY_Page_Up:
                    command = code_edit_key::page_up;
                    break;
                case GDK_KEY_Page_Down:
                    command = code_edit_key::page_down;
                    break;
                case GDK_KEY_BackSpace:
                    command = code_edit_key::backspace;
                    break;
                case GDK_KEY_Delete:
                    command = code_edit_key::delete_forward;
                    break;
                case GDK_KEY_Return:
                    command = code_edit_key::enter;
                    break;
                case GDK_KEY_Tab:
                    command = code_edit_key::tab;
                    break;
                case GDK_KEY_Escape:
                    command = code_edit_key::escape;
                    break;
                default:
                    return false;
                }
            return editor->on_native_key(command, shift);
        }
        return false;
    }
} // namespace
namespace linux::wayland
{
    bool table_key(native::table_view &owner,
                   guint key,
                   GdkModifierType modifiers) {
        if (!owner.get_input_enabled())
            return true;
        using native::table_navigation;
        table_navigation action;
        switch (key) {
        case GDK_KEY_Up:
            action = table_navigation::up;
            break;
        case GDK_KEY_Down:
            action = table_navigation::down;
            break;
        case GDK_KEY_Home:
            action = table_navigation::home;
            break;
        case GDK_KEY_End:
            action = table_navigation::end;
            break;
        case GDK_KEY_Page_Up:
            action = table_navigation::page_up;
            break;
        case GDK_KEY_Page_Down:
            action = table_navigation::page_down;
            break;
        case GDK_KEY_Left:
            action = table_navigation::collapse;
            break;
        case GDK_KEY_Right:
            action = table_navigation::expand;
            break;
        case GDK_KEY_Return:
            action = table_navigation::activate;
            break;
        case GDK_KEY_space:
            action = table_navigation::toggle;
            break;
        default: {
            if ((modifiers & GDK_CONTROL_MASK) &&
                gdk_keyval_to_lower(key) == GDK_KEY_a)
                action = table_navigation::select_all;
            else {
                const auto character = gdk_keyval_to_unicode(key);
                if (!character || !g_unichar_isprint(character) ||
                    (modifiers & (GDK_CONTROL_MASK | GDK_ALT_MASK |
                                  GDK_SUPER_MASK)))
                    return false;
                char text[8] = {};
                const int count = g_unichar_to_utf8(character, text);
                owner.on_native_type_text(std::string(text, count));
                return true;
            }
        }
        }
        owner.on_native_navigation(action, modifiers & GDK_SHIFT_MASK);
        return true;
    }
    void install_input(native::wnd &owner, GtkWidget *widget) {
        gtk_widget_set_focusable(widget, true);
        auto *motion = gtk_event_controller_motion_new();
        gtk_event_controller_set_propagation_phase(
            GTK_EVENT_CONTROLLER(motion), GTK_PHASE_TARGET);
        g_signal_connect(
            motion,
            "motion",
            G_CALLBACK(+[](GtkEventControllerMotion *,
                           double x,
                           double y,
                           gpointer data) {
                auto &owner = *static_cast<native::wnd *>(data);
                if (owner.get_input_enabled()) {
                    auto &peer = state(owner);
                    if (auto *table =
                            dynamic_cast<native::table_view *>(&owner);
                        table && peer.dragging_scrollbar)
                        native::detail::drag_table_scrollbar(
                            *table,
                            position(x, y),
                            peer.horizontal_scrollbar,
                            peer.scrollbar_grab);
                    else
                        owner.on_native_mouse_move(position(x, y));
                }
            }),
            &owner);
        g_signal_connect(
            motion,
            "enter",
            G_CALLBACK(+[](GtkEventControllerMotion *,
                           double x,
                           double y,
                           gpointer data) {
                static_cast<native::wnd *>(data)->on_native_mouse_enter(
                    position(x, y));
            }),
            &owner);
        g_signal_connect(
            motion,
            "leave",
            G_CALLBACK(+[](GtkEventControllerMotion *, gpointer data) {
                static_cast<native::wnd *>(data)->on_native_mouse_leave(
                    native::point());
            }),
            &owner);
        gtk_widget_add_controller(widget, motion);
        auto *gesture = gtk_gesture_click_new();
        gtk_event_controller_set_propagation_phase(
            GTK_EVENT_CONTROLLER(gesture), GTK_PHASE_TARGET);
        gtk_gesture_single_set_button(GTK_GESTURE_SINGLE(gesture), 0);
        g_signal_connect(gesture,
                         "pressed",
                         G_CALLBACK(+[](GtkGestureClick *g,
                                        int n,
                                        double x,
                                        double y,
                                        gpointer d) {
                             click(g, n, x, y, d, true);
                         }),
                         &owner);
        g_signal_connect(gesture,
                         "released",
                         G_CALLBACK(+[](GtkGestureClick *g,
                                        int n,
                                        double x,
                                        double y,
                                        gpointer d) {
                             click(g, n, x, y, d, false);
                         }),
                         &owner);
        gtk_widget_add_controller(widget,
                                  GTK_EVENT_CONTROLLER(gesture));
        auto *scroll = gtk_event_controller_scroll_new(
            GTK_EVENT_CONTROLLER_SCROLL_BOTH_AXES);
        gtk_event_controller_set_propagation_phase(
            GTK_EVENT_CONTROLLER(scroll), GTK_PHASE_TARGET);
        g_signal_connect(
            scroll,
            "scroll",
            G_CALLBACK(+[](GtkEventControllerScroll *,
                           double dx,
                           double dy,
                           gpointer data) -> gboolean {
                auto &owner = *static_cast<native::wnd *>(data);
                if (!owner.get_input_enabled())
                    return false;
                const int delta =
                    dy ? (dy > 0 ? -1 : 1) : (dx > 0 ? -1 : 1);
                if (auto *icons =
                        dynamic_cast<native::icon_view *>(&owner))
                    icons->on_native_scroll(delta);
                else if (auto *table =
                             dynamic_cast<native::table_view *>(&owner))
                    table->on_native_scroll(
                        static_cast<std::size_t>(std::max<std::int64_t>(
                            0,
                            static_cast<std::int64_t>(
                                table->get_vertical_scroll_row()) -
                                delta * 3)),
                        table->get_horizontal_scroll_offset());
                else
                    owner.on_native_mouse_wheel(
                        native::mouse_wheel_event(
                            native::point(),
                            delta,
                            dy ? native::wheel_direction::vertical
                               : native::wheel_direction::horizontal));
                return true;
            }),
            &owner);
        gtk_widget_add_controller(widget, scroll);
        auto *focus = gtk_event_controller_focus_new();
        gtk_event_controller_set_propagation_phase(
            GTK_EVENT_CONTROLLER(focus), GTK_PHASE_TARGET);
        g_signal_connect(
            focus,
            "enter",
            G_CALLBACK(+[](GtkEventControllerFocus *, gpointer data) {
                static_cast<native::wnd *>(data)->on_native_focus(true);
            }),
            &owner);
        g_signal_connect(
            focus,
            "leave",
            G_CALLBACK(+[](GtkEventControllerFocus *, gpointer data) {
                static_cast<native::wnd *>(data)->on_native_focus(
                    false);
            }),
            &owner);
        gtk_widget_add_controller(widget, focus);
        auto *keys = gtk_event_controller_key_new();
        gtk_event_controller_set_propagation_phase(
            GTK_EVENT_CONTROLLER(keys), GTK_PHASE_TARGET);
        g_signal_connect(keys,
                         "key-pressed",
                         G_CALLBACK(+[](GtkEventControllerKey *,
                                        guint key,
                                        guint,
                                        GdkModifierType mods,
                                        gpointer data) -> gboolean {
                             auto &owner =
                                 *static_cast<native::wnd *>(data);
                             return owner.get_input_enabled() &&
                                    command(owner, key, mods);
                         }),
                         &owner);
        auto *im = gtk_im_multicontext_new();
        gtk_im_context_set_client_widget(im, widget);
        gtk_event_controller_key_set_im_context(
            GTK_EVENT_CONTROLLER_KEY(keys), im);
        g_signal_connect(
            im,
            "commit",
            G_CALLBACK(
                +[](GtkIMContext *, const char *text, gpointer data) {
                    if (auto *editor =
                            dynamic_cast<native::code_edit *>(
                                static_cast<native::wnd *>(data)))
                        editor->on_native_text_input(text);
                }),
            &owner);
        g_object_unref(im);
        gtk_widget_add_controller(widget, keys);
    }
} // namespace linux::wayland
