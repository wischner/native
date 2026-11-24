//
// Checks OPEN LOOK toolbar presentation, native menu grouping, and
// physical key delivery through the XView Panel notification chain.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>
#include "../lib/native/toolkits/openlook/globals.h"

#include <X11/keysym.h>
#include <array>
#include <cstdlib>
#include <stdexcept>

namespace
{
    namespace ol = linux::openlook;

    void expect(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void attach(native::wnd &control, native::wnd &parent) {
        control.set_parent(&parent);
        control.create();
        control.show();
    }

    // Post to the native Panel, rather than calling portable input hooks.
    void key(Panel panel, KeySym symbol, unsigned modifiers = 0) {
        for (int type : {KeyPress, KeyRelease}) {
            XEvent raw{};
            raw.xkey.type = type;
            raw.xkey.display = ol::cached_display;
            raw.xkey.window = xv_get(panel, XV_XID);
            raw.xkey.keycode = XKeysymToKeycode(ol::cached_display, symbol);
            raw.xkey.state = modifiers;
            Event event{};
            event_set_id(&event, symbol < 128 ? symbol : 0);
            if (type == KeyRelease)
                event_set_up(&event);
            event_set_xevent(&event, &raw);
            notify_post_event(panel, reinterpret_cast<Notify_event>(&event),
                              NOTIFY_SAFE);
            notify_dispatch();
        }
    }
}

void check_openlook_desktop(native::app_wnd &owner) {
    native::modeless_wnd dialog(owner, "Desktop controls", 40, 40, 520, 250);
    dialog.menu << "Format" << (native::menu_items("Style")
        << native::menu_separator << "Alignment");
    dialog.create();
    dialog.show();
    auto *menu = ol::menu_bindings.object_from_handle(dialog.menu.id());
    expect(menu && menu->menus.size() == 1, "native menu exists");
    expect(xv_get(menu->menus[0], MENU_NITEMS) == 3,
           "menu groups retain a visible spacer row");
    const auto spacer = xv_get(menu->menus[0], MENU_NTH_ITEM, 2);
    expect(xv_get(spacer, MENU_INACTIVE), "menu spacer cannot be activated");
    native::text_edit first("", native::text_edit_mode::single_line,
                             20, 20, 200, 24);
    native::text_edit second("", native::text_edit_mode::single_line,
                              20, 55, 200, 24);
    native::check check("Option", 20, 90, 120, 24);
    native::button button("Apply", 20, 125, 96, 28);
    const std::array<native::wnd *, 4> controls{&first, &second, &check, &button};
    unsigned order = 1;
    for (auto *control : controls) {
        control->set_tab_order(order++);
        attach(*control, dialog);
    }
    Panel panel = ol::parent_panel(&first);
    first.focus();
    const auto caret = [&] {
        return ol::wnd_bindings.object_from_handle(xv_get(panel, PANEL_CARET_ITEM));
    };
    for (std::size_t i = 0; i < controls.size(); ++i) {
        key(panel, XK_Tab);
        expect(caret() == controls[(i + 1) % controls.size()],
               "physical Tab reaches native text, check and button items");
    }
    key(panel, XK_Tab, ShiftMask);
    expect(caret() == &button, "Shift+Tab wraps to the native button");
    int activations = 0;
    button.on_key.connect([&](native::key_event event) {
        if (event.key != native::key_code::enter)
            return false;
        if (event.action == native::key_action::press)
            ++activations;
        return true;
    });
    key(panel, XK_Return);
    expect(activations == 1, "Enter reaches the focused button exactly once");
    first.set_enter_as_tab(false);
    first.on_key.connect([&](native::key_event event) {
        if (event.key != native::key_code::enter)
            return false;
        if (event.action == native::key_action::press)
            ++activations;
        return true;
    });
    first.focus();
    key(panel, XK_Return);
    expect(activations == 2 && caret() == &first,
           "an Enter action keeps the single-line field focused");

    native::combo_box combo({"100%", "150%"},
        native::combo_box_style::drop_down_list, 250, 20, 140, 24);
    attach(combo, dialog);
    const auto centered = [&] {
        auto *state = ol::combo_box_bindings.object_from_handle(&combo);
        const int actual_y = xv_get(state->choice, XV_Y);
        const int actual_h = xv_get(state->choice, XV_HEIGHT);
        const auto bounds = combo.get_bounds();
        expect(std::abs(2 * actual_y + actual_h -
                        (2 * bounds.y1() + bounds.h())) <= 1,
               "native choice stack is vertically centered in its bounds");
    };
    centered();
    combo.set_bounds(native::rect(250, 55, 160, 30));
    centered();
    combo.set_items({"50%", "100%", "200%"});
    centered();
    dialog.destroy();
}
