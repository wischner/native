//
// Translates compositor XKB physical key names without Xlib or key
// symbols. Native GTK editing remains responsible for layout-dependent
// text and IME.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//
#include "globals.h"
#include "../../input_state.h"
#include <xkbcommon/xkbcommon.h>
#include <cstring>
namespace
{
    native::key_code physical(GtkEventController *controller,
                              guint code) {
        auto *device =
            gtk_event_controller_get_current_event_device(controller);
        if (!device || !GDK_IS_WAYLAND_DEVICE(device))
            return native::key_code::unknown;
        auto *keymap = gdk_wayland_device_get_xkb_keymap(device);
        const char *name =
            keymap ? xkb_keymap_key_get_name(keymap, code) : nullptr;
        if (!name)
            return native::key_code::unknown;
        struct key
        {
            const char *name;
            unsigned scan;
            bool extended;
        };
        static constexpr key keys[] = {
            {"ESC", 1, false},   {"AE01", 2, false},
            {"AE02", 3, false},  {"AE03", 4, false},
            {"AE04", 5, false},  {"AE05", 6, false},
            {"AE06", 7, false},  {"AE07", 8, false},
            {"AE08", 9, false},  {"AE09", 10, false},
            {"AE10", 11, false}, {"AE11", 12, false},
            {"AE12", 13, false}, {"BKSP", 14, false},
            {"TAB", 15, false},  {"AD01", 16, false},
            {"AD02", 17, false}, {"AD03", 18, false},
            {"AD04", 19, false}, {"AD05", 20, false},
            {"AD06", 21, false}, {"AD07", 22, false},
            {"AD08", 23, false}, {"AD09", 24, false},
            {"AD10", 25, false}, {"AD11", 26, false},
            {"AD12", 27, false}, {"RTRN", 28, false},
            {"LCTL", 29, false}, {"AC01", 30, false},
            {"AC02", 31, false}, {"AC03", 32, false},
            {"AC04", 33, false}, {"AC05", 34, false},
            {"AC06", 35, false}, {"AC07", 36, false},
            {"AC08", 37, false}, {"AC09", 38, false},
            {"AC10", 39, false}, {"AC11", 40, false},
            {"TLDE", 41, false}, {"LFSH", 42, false},
            {"BKSL", 43, false}, {"AB01", 44, false},
            {"AB02", 45, false}, {"AB03", 46, false},
            {"AB04", 47, false}, {"AB05", 48, false},
            {"AB06", 49, false}, {"AB07", 50, false},
            {"AB08", 51, false}, {"AB09", 52, false},
            {"AB10", 53, false}, {"RTSH", 54, false},
            {"KPMU", 55, false}, {"LALT", 56, false},
            {"SPCE", 57, false}, {"CAPS", 58, false},
            {"FK01", 59, false}, {"FK02", 60, false},
            {"FK03", 61, false}, {"FK04", 62, false},
            {"FK05", 63, false}, {"FK06", 64, false},
            {"FK07", 65, false}, {"FK08", 66, false},
            {"FK09", 67, false}, {"FK10", 68, false},
            {"NMLK", 69, false}, {"SCLK", 70, false},
            {"KP7", 71, false},  {"KP8", 72, false},
            {"KP9", 73, false},  {"KPSU", 74, false},
            {"KP4", 75, false},  {"KP5", 76, false},
            {"KP6", 77, false},  {"KPAD", 78, false},
            {"KP1", 79, false},  {"KP2", 80, false},
            {"KP3", 81, false},  {"KP0", 82, false},
            {"KPDL", 83, false}, {"FK11", 87, false},
            {"FK12", 88, false}, {"KPEN", 28, true},
            {"RCTL", 29, true},  {"RALT", 56, true},
            {"HOME", 71, true},  {"UP", 72, true},
            {"PGUP", 73, true},  {"LEFT", 75, true},
            {"RGHT", 77, true},  {"END", 79, true},
            {"DOWN", 80, true},  {"PGDN", 81, true},
            {"INS", 82, true},   {"DELE", 83, true},
            {"LWIN", 91, true},  {"RWIN", 92, true},
            {"KPDV", 53, true},  {"PRSC", 55, true}};
        for (const auto &key : keys)
            if (std::strcmp(name, key.name) == 0)
                return native::detail::pc_key(key.scan, key.extended);
        return native::key_code::unknown;
    }
    native::wnd &focused(native::wnd &owner,
                         GtkEventController *controller) {
        auto *root = gtk_widget_get_root(
            gtk_event_controller_get_widget(controller));
        for (auto *widget = root ? gtk_root_get_focus(root) : nullptr;
             widget;
             widget = gtk_widget_get_parent(widget))
            if (auto *target =
                    linux::wayland::wnd_bindings.object_from_handle(
                        widget))
                return *target;
        return owner;
    }
    native::key_modifiers modifiers(GdkModifierType state) {
        using native::key_modifiers;
        auto result = key_modifiers::none;
        if (state & GDK_SHIFT_MASK)
            result = result | key_modifiers::shift;
        if (state & GDK_CONTROL_MASK)
            result = result | key_modifiers::ctrl;
        if (state & GDK_ALT_MASK)
            result = result | key_modifiers::alt;
        if (state & GDK_SUPER_MASK)
            result = result | key_modifiers::meta;
        if (state & GDK_LOCK_MASK)
            result = result | key_modifiers::caps_lock;
        return result;
    }
} // namespace
namespace linux::wayland
{
    void install_keyboard(native::wnd &owner, GtkWidget *widget) {
        if (GTK_IS_WINDOW(widget)) {
            auto *keys = gtk_event_controller_key_new();
            gtk_event_controller_set_propagation_phase(
                keys, GTK_PHASE_CAPTURE);
            g_signal_connect(
                keys,
                "key-pressed",
                G_CALLBACK(+[](GtkEventControllerKey *controller,
                               guint,
                               guint code,
                               GdkModifierType mods,
                               gpointer data) -> gboolean {
                    auto &owner =
                        focused(*static_cast<native::wnd *>(data),
                                GTK_EVENT_CONTROLLER(controller));
                    if (!owner.get_input_enabled())
                        return true;
                    auto key = physical(
                        GTK_EVENT_CONTROLLER(controller), code);
                    if (key == native::key_code::unknown)
                        return false;
                    return owner.on_native_key(native::key_event{
                        key,
                        native::key_action::press,
                        native::detail::key_held(owner, key),
                        modifiers(mods)});
                }),
                &owner);
            g_signal_connect(
                keys,
                "key-released",
                G_CALLBACK(+[](GtkEventControllerKey *controller,
                               guint,
                               guint code,
                               GdkModifierType mods,
                               gpointer data) {
                    auto &owner =
                        focused(*static_cast<native::wnd *>(data),
                                GTK_EVENT_CONTROLLER(controller));
                    auto key = physical(
                        GTK_EVENT_CONTROLLER(controller), code);
                    if (key != native::key_code::unknown)
                        owner.on_native_key(native::key_event{
                            key,
                            native::key_action::release,
                            false,
                            modifiers(mods)});
                }),
                &owner);
            gtk_widget_add_controller(widget, keys);
        }
        auto *focus = gtk_event_controller_focus_new();
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
    }
} // namespace linux::wayland
