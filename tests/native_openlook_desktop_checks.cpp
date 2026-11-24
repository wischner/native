//
// Checks OPEN LOOK toolbar presentation, native menu grouping, and
// physical key delivery through the XView Panel notification chain.
//
// MIT License (see: LICENSE)
// Copyright (C) 2026 Tomaz Stih
//

#include <native.h>
#include "../lib/native/toolkits/openlook/globals.h"
#include "../lib/native/post_backend.h"

#define class xview_menu_class
#include <xview_private/om_impl.h>
#undef class

#include <X11/keysym.h>
#include <dlfcn.h>
#include <X11/Xutil.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <stdexcept>

extern "C" Notify_value xv_input_pending(Display *, int);

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

    // Pump the real mapping/focus notifications and posted work in the fixture.
    void pump() {
        XSync(ol::cached_display, False);
        xv_input_pending(ol::cached_display, ConnectionNumber(ol::cached_display));
        notify_dispatch();
        native::detail::drain_posted_work();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    // Desktop separators are blank half-button gaps, including on repaint.
    void check_popup_spacing(Menu menu, Panel bar) {
        void *library = dlopen("libXtst.so.6", RTLD_NOW | RTLD_LOCAL);
        using key_proc = int (*)(Display *, unsigned, Bool, unsigned long);
        auto send = library ? reinterpret_cast<key_proc>(
            dlsym(library, "XTestFakeKeyEvent")) : nullptr;
        expect(send, "XTest can dismiss the rendered popup");
        using motion_proc = int (*)(Display *, int, int, int, unsigned long);
        auto motion = reinterpret_cast<motion_proc>(
            dlsym(library, "XTestFakeMotionEvent"));
        expect(motion, "XTest can move through the rendered popup");
        auto button = reinterpret_cast<key_proc>(
            dlsym(library, "XTestFakeButtonEvent"));
        expect(button, "XTest can hold the menu button during pointer tracking");
        std::array<int, 4> selected{-1, -1, -1, -1};
        pump();
        Event event{};
        event_set_window(&event, bar);
        event_set_id(&event, MS_RIGHT);
        event_set_down(&event);
        event_set_x(&event, 10);
        event_set_y(&event, 30);
        menu_show(menu, bar, &event, nullptr);
        pump();
        Window root = DefaultRootWindow(ol::cached_display);
        Window parent = None, returned_root = None, *children = nullptr;
        unsigned count = 0;
        XQueryTree(ol::cached_display, root, &returned_root, &parent,
                   &children, &count);
        bool blank_gap = false;
        // The menu is above its shadow and all ordinary application windows.
        for (unsigned i = count; i > 0; --i) {
            XWindowAttributes attributes{};
            XGetWindowAttributes(ol::cached_display, children[i - 1],
                                 &attributes);
            if (!attributes.override_redirect ||
                attributes.map_state != IsViewable ||
                attributes.width < 32 || attributes.height < 40)
                continue;
            // Re-expose the cached popup to check its native repaint path.
            XClearArea(ol::cached_display, children[i - 1], 0, 0, 0, 0, True);
            pump();
            XImage *image = XGetImage(ol::cached_display, children[i - 1],
                0, 0, attributes.width, attributes.height, AllPlanes, ZPixmap);
            if (image) {
                const auto *state = MENU_PRIVATE(menu);
                const int border = state->xview_menu_class == MENU_COMMAND ? 2 : 6;
                const int row = state->default_image.height;
                const int gap = Button_Height(state->ginfo) / 2;
                const auto background = XGetPixel(image, 8, border + row);
                blank_gap = attributes.height == 2 * border + 2 * row + gap;
                for (int y = border + row; y < border + row + gap; ++y)
                    for (int x = border; x < attributes.width - border; ++x)
                        blank_gap = blank_gap &&
                            XGetPixel(image, x, y) == background;
                if (const char *directory = std::getenv("NATIVE_POPUP_CAPTURE")) {
                    native::img capture(attributes.width, attributes.height);
                    for (int y = 0; y < attributes.height; ++y)
                        for (int x = 0; x < attributes.width; ++x) {
                            const auto pixel = XGetPixel(image, x, y);
                            capture.pixels()[y * attributes.width + x] =
                                native::rgba((pixel >> 16) & 255,
                                    (pixel >> 8) & 255, pixel & 255, 255);
                        }
                    const bool toggle = xv_get(menu, MENU_CLASS) == MENU_TOGGLE;
                    capture.save(std::filesystem::path(directory) /
                        (toggle ? "toggle-menu.png" : "command-menu.png"));
                }
                XDestroyImage(image);
                // Hit the row below the compact gap, then navigate across it.
                const int x = attributes.x + attributes.width / 2;
                // menu_show received MENU-down; supply the matching physical
                // button state so XView receives drag events from the server.
                button(ol::cached_display, 3, True, 0);
                motion(ol::cached_display, -1, x,
                    attributes.y + border + row + gap + row / 2, 0);
                for (int attempt = 0; attempt < 20; ++attempt) pump();
                selected[0] = state->curitem;
                const auto up = XKeysymToKeycode(ol::cached_display, XK_Up);
                send(ol::cached_display, up, True, 0);
                send(ol::cached_display, up, False, 0);
                for (int attempt = 0; attempt < 20; ++attempt) pump();
                selected[1] = state->curitem;
                const auto down = XKeysymToKeycode(ol::cached_display, XK_Down);
                send(ol::cached_display, down, True, 0);
                send(ol::cached_display, down, False, 0);
                for (int attempt = 0; attempt < 20; ++attempt) pump();
                selected[2] = state->curitem;
                motion(ol::cached_display, -1, x,
                    attributes.y + border + row + gap / 2, 0);
                for (int attempt = 0; attempt < 20; ++attempt) pump();
                // XView tracks the hovered inactive row but cannot activate it.
                selected[3] = state->curitem;
            }
            break;
        }
        if (children) XFree(children);
        // Dismiss the popup through the real keyboard path before proceeding.
        const auto escape = XKeysymToKeycode(ol::cached_display, XK_Escape);
        send(ol::cached_display, escape, True, 0);
        send(ol::cached_display, escape, False, 0);
        pump();
        button(ol::cached_display, 3, False, 0);
        pump();
        dlclose(library);
        if (selected[0] != 3 || selected[1] != 1 ||
            selected[2] != 3 || selected[3] != 2)
            throw std::runtime_error("menu hit after pointer/up/down/gap: " +
                std::to_string(selected[0]) + "/" + std::to_string(selected[1]) +
                "/" + std::to_string(selected[2]) + "/" + std::to_string(selected[3]));
        expect(blank_gap, "popup separator is a blank half-button desktop gap");
    }

    class blocked_text final : public native::text_edit
    {
    public:
        bool get_input_enabled() const override { return false; }
    };

    void check_initial_focus(native::app_wnd &owner) {
        native::modeless_wnd dialog(owner, "Initial dialog focus", 80, 80, 420, 200);
        native::text_edit later("", native::text_edit_mode::single_line,
                                20, 60, 180, 24);
        native::text_edit first("", native::text_edit_mode::single_line,
                                20, 20, 180, 24);
        native::text_edit hidden, uncreated;
        blocked_text disabled;
        later.set_tab_order(20);
        first.set_tab_order(10);
        disabled.set_tab_order(1);
        hidden.set_tab_order(2);
        uncreated.set_tab_order(3);
        uncreated.set_parent(&dialog);
        for (int cycle = 0; cycle < 2; ++cycle) {
            dialog.create();
            attach(later, dialog); // Creation order must not select this field.
            attach(first, dialog);
            attach(disabled, dialog);
            hidden.set_parent(&dialog);
            hidden.create(); // Explicitly hidden lower-order stop.
            dialog.show();
            const auto panel = ol::parent_panel(&first);
            const auto caret = [&] {
                return ol::wnd_bindings.object_from_handle(
                    xv_get(panel, PANEL_CARET_ITEM));
            };
            const auto input_window = xv_get(
                ol::window_state(&dialog)->paint_window, XV_XID);
            bool ready = false;
            for (int attempt = 0; attempt < 500; ++attempt) {
                pump();
                Window focused = None;
                int revert = 0;
                XGetInputFocus(ol::cached_display, &focused, &revert);
                if (caret() == &first && focused == input_window) {
                    ready = true;
                    break;
                }
            }
            expect(ready,
                   "opening/reopening focuses the lowest eligible tab order natively");
            later.focus();
            dialog.show(); // Raising an open dialog must preserve the chosen field.
            for (int i = 0; i < 20; ++i) pump();
            expect(caret() == &later, "raising an open dialog does not restart traversal");
            dialog.destroy();
        }
        dialog.create();
        dialog.show();
        dialog.destroy(); // Mapping callbacks must not retain a destroyed dialog.
        for (int i = 0; i < 20; ++i) pump();
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
    check_initial_focus(owner);
    native::modeless_wnd dialog(owner, "Desktop controls", 40, 40, 520, 250);
    dialog.menu << "Format" << (native::menu_items("Style")
        << native::menu_separator << "Alignment");
    dialog.menu << "View" << (native::menu_items("Zoom")
        << native::menu_separator
        << native::menu_check_item{51, "Grid lines", true});
    dialog.create();
    dialog.show();
    auto *menu = ol::menu_bindings.object_from_handle(dialog.menu.id());
    expect(menu && menu->menus.size() == 2, "native menu exists");
    expect(xv_get(menu->menus[0], MENU_NITEMS) == 3,
           "menu groups retain a separator row");
    const auto menu_button = xv_get(menu->bar, PANEL_FIRST_ITEM);
    expect(!xv_get(menu_button, PANEL_ACCEPT_KEYSTROKE),
           "menu buttons do not acquire keyboard focus arrows");
    const auto separator = xv_get(menu->menus[0], MENU_NTH_ITEM, 2);
    expect(xv_get(separator, MENU_INACTIVE),
           "menu separator cannot be activated");
    for (int cycle = 0; cycle < 2; ++cycle)
        for (const auto popup : menu->menus)
            check_popup_spacing(popup, menu->bar);

    native::img toolbar_gap(32, 32);
    const native::rgba paper(255, 255, 255, 255);
    std::fill_n(toolbar_gap.pixels(), 32 * 32, paper);
    auto theme = native::theme::create(toolbar_gap.get_gpx());
    theme->draw_toolbar_separator(native::rect(0, 0, 32, 32),
                                  native::separator_orientation::vertical);
    expect(std::all_of(toolbar_gap.pixels(), toolbar_gap.pixels() + 32 * 32,
                       [paper](native::rgba pixel) { return pixel == paper; }),
           "toolbar groups retain whitespace without a separator line");
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
    button.set_tab_order(0);
    expect(!xv_get(ol::wnd_bindings.handle_from_object(&button),
                  PANEL_ACCEPT_KEYSTROKE),
           "clearing a live button tab order removes native focus eligibility");
    expect(caret() != &button, "clearing tab order releases the native caret");
    button.set_tab_order(4);
    button.focus();
    expect(caret() == &button, "restoring tab order restores native button focus");
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
    auto *combo_state = ol::combo_box_bindings.object_from_handle(&combo);
    expect(!xv_get(combo_state->choice, PANEL_ACCEPT_KEYSTROKE),
           "unordered toolbar-style choices cannot acquire a focus arrow");
    first.focus();
    combo.focus();
    expect(caret() == &first,
           "an unordered choice does not steal the native caret");
    combo.set_tab_order(5);
    expect(xv_get(combo_state->choice, PANEL_ACCEPT_KEYSTROKE),
           "a live explicit tab order enables choice focus");
    combo.set_items({"75%", "100%"});
    expect(xv_get(combo_state->choice, PANEL_ACCEPT_KEYSTROKE),
           "choice replacement preserves explicit focus eligibility");
    combo.set_tab_order(0);
    combo.set_items({"100%", "200%"});
    expect(!xv_get(combo_state->choice, PANEL_ACCEPT_KEYSTROKE),
           "choice replacement preserves exclusion from keyboard focus");
    native::button unordered_button("Tool", 250, 100, 80, 28);
    native::check unordered_check("Tool option", 250, 140, 140, 24);
    for (auto *control : std::array<native::wnd *, 2>{
             &unordered_button, &unordered_check}) {
        attach(*control, dialog);
        expect(!xv_get(ol::wnd_bindings.handle_from_object(control),
                      PANEL_ACCEPT_KEYSTROKE),
               "unordered buttons and checks remain outside native focus");
    }
    first.focus();
    const auto check_item = ol::wnd_bindings.handle_from_object(&unordered_check);
    const auto *check_rect = reinterpret_cast<const Rect *>(
        xv_get(check_item, PANEL_ITEM_RECT));
    Event click{};
    event_set_id(&click, MS_LEFT);
    event_set_action(&click, ACTION_SELECT);
    event_set_x(&click, check_rect->r_left + 5);
    event_set_y(&click, check_rect->r_top + check_rect->r_height / 2);
    panel_handle_event(check_item, &click);
    event_set_up(&click);
    panel_handle_event(check_item, &click);
    expect(unordered_check.get_checked(), "unordered checks retain mouse activation");
    const auto frame = ol::window_state(&dialog)->frame;
    const auto indicator = xv_get(frame, FRAME_FOCUS_WIN);
    expect(!xv_get(indicator, XV_SHOW),
           "mouse activation of an unordered choice leaves no focus arrow");
    dialog.destroy();
}
